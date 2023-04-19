// The MIT License( MIT )
//
// Copyright( c ) 2020-2021 Scott Aron Bloom
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files( the "Software" ), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sub-license, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <QApplication>
#include <QProcess>
#include <QStringList>
#include <QRegularExpression>
#include <cstring>
#include <iostream>
#include <optional>

int QStringNCmp( const QString & lhs, const QString & rhs, size_t sz, Qt::CaseSensitivity caseSensitivity )
{
    auto realLHS = ( ( caseSensitivity == Qt::CaseSensitivity::CaseSensitive ) ? lhs.toLower() : lhs ).left( (int)sz ).toStdString();
    auto realRHS = ( ( caseSensitivity == Qt::CaseSensitivity::CaseSensitive ) ? rhs.toLower() : rhs ).left( (int)sz ).toStdString();
    return std::strncmp( realLHS.c_str(), realRHS.c_str(), sz );
}

void Usage()
{
    std::cout
        << "RunTee -err_prefix <prefix> -out_prefix <prefix> Program <Program Args>\n"
        << "  -err_prefix <prefix> - The prefix will be added to every line out output for stderr (default \"Error: \")\n"
        << " - out_prefix <prefix> -The prefix will be added to every line out output for stdout(default empty)" << std::endl;
        ;
}

void readData( const QString & prefix, QString & currLine, const QString & newData, bool isError )
{
    if( currLine.isEmpty() )
        currLine = prefix;
    currLine.append( newData );
    static QRegularExpression regex( "((?:\\r)?\\n)" );
    if ( !prefix.isEmpty() )
        currLine.replace( regex, "\\1" + prefix );

    auto lastEOL = currLine.lastIndexOf( "\n" );
    if( lastEOL >= 0 )
    {
        ( isError ? std::cerr : std::cout ) << qPrintable( currLine.left( lastEOL ) );
        currLine.remove( 0, lastEOL + 1 );
    }
    currLine.clear();
}

void flushData( const QString & prefix, QString & currLine, const QString & newData, bool isError )
{
    currLine.append( newData );
    if( !currLine.isEmpty() && !currLine.endsWith( "\n" ) )
        currLine += "\n";
    if( currLine.isEmpty() )
        return;
    readData( prefix, currLine, QString(), isError );
}

QString toString( QProcess::ProcessError error )
{
    QString retVal;
    switch( error )
    {
        case QProcess::ProcessError::FailedToStart: retVal = "FailedToStart"; break;
        case QProcess::ProcessError::Crashed: retVal = "Crashed"; break;
        case QProcess::ProcessError::Timedout: retVal = "Timedout"; break;
        case QProcess::ProcessError::WriteError: retVal = "WriteError"; break;
        case QProcess::ProcessError::ReadError: retVal = "ReadError"; break;
        case QProcess::ProcessError::UnknownError: retVal = "UnknownError"; break;
    }
    return retVal;
}

QString toString( QProcess::ProcessState newState )
{
    QString retVal;
    switch( newState )
    {
        case QProcess::ProcessState::NotRunning: retVal = "NotRunning"; break;
        case QProcess::ProcessState::Starting: retVal = "Starting"; break;
        case QProcess::ProcessState::Running: retVal = "Running"; break;
    }
    return retVal;
}

QString toString( QProcess::ExitStatus newState )
{
    QString retVal;
    switch( newState )
    {
        case QProcess::ExitStatus::NormalExit: retVal = "NormalExit"; break;
        case QProcess::ExitStatus::CrashExit: retVal = "CrashExit"; break;
    }
    return retVal;
}

int main( int argc, char ** argv )
{
    QApplication appl( argc, argv );
    appl.setApplicationName( "RunTee" );
    appl.setApplicationVersion( "0.1" );
    appl.setOrganizationName( "Scott Aron Bloom" );
    appl.setOrganizationDomain( "www.towel42.com" );

    //Q_INIT_RESOURCE( MainLib );

    // usage
    // RunTee -err_prefix <prefix> -out_prefix <prefix> program <args>
    //   -err_prefix <prefix> - The prefix will be added to every line out output for stderr (default "Error: ")
    //   -out_prefix <prefix> - The prefix will be added to every line out output for stdout (default empty)

    QString errPrefix = "Error: ";
    QString outPrefix;

    std::optional< QString > program;
    QStringList args;

    for( int ii = 1; ii < argc; ++ii )
    {
        if( QStringNCmp( "-err_prefix", argv[ ii ], strlen( argv[ ii ] ), Qt::CaseInsensitive ) == 0 )
        {
            if( ii == ( argc - 1 ) )
            {
                std::cerr << "-err_prefix requires <prefix> parameter\n";
                Usage();
                return -1;
            }
            ii++;
            errPrefix = argv[ ii ];
        }
        else if( QStringNCmp( "-out_prefix", argv[ ii ], strlen( argv[ ii ] ), Qt::CaseInsensitive ) == 0 )
        {
            if( ii == ( argc - 1 ) )
            {
                std::cerr << "-out_prefix requires <prefix> parameter\n";
                Usage();
                return -1;
            }
            ii++;
            outPrefix = argv[ ii ];
        }
        else if( !program.has_value() )
        {
            program = argv[ ii ];
        }
        else
            args << argv[ ii ];
    }

    if( !program.has_value() )
    {
        std::cerr << "Program is not set:\n";
        Usage();
        return -1;
    }
    
    QProcess process;
    QString currErrorLine;
    QObject::connect( &process, &QProcess::readyReadStandardError,
                      [&process, errPrefix, &currErrorLine]()
    {
        readData( errPrefix, currErrorLine, process.readAllStandardError(), true );
    } );

    QString currOutLine;
    QObject::connect( &process, &QProcess::readyReadStandardOutput,
                      [&process, outPrefix, &currOutLine]()
    {
        readData( outPrefix, currOutLine, process.readAllStandardOutput(), false );
    } );

    QObject::connect( &process, &QProcess::errorOccurred,
                      [&process]( QProcess::ProcessError error )
    {
        auto msg = QString( "%1(%2): %3" ).arg( toString( error ) ).arg( error ).arg( process.errorString() );
        std::cerr << qPrintable( msg ) << std::endl;
        return -1;
    } );

    process.start( program.value(), args );
    if( !process.waitForStarted() )
    {
        std::cerr << "Error starting program: '" << qPrintable( program.value() ) << "' with args: " << qPrintable( args.join( " " ) ) << "\n";
        return -1;
    }

    if( !process.waitForFinished() )
    {
        std::cerr << "Finished with error\n";
        return -1;
    }

    flushData( outPrefix, currOutLine, process.readAllStandardOutput(), false );
    flushData( errPrefix, currErrorLine, process.readAllStandardError(), true );
    return 0;
}
