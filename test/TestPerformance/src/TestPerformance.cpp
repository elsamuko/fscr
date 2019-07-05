#define BOOST_TEST_MODULE Performance

#include "boost/iostreams/device/mapped_file.hpp"
#include "boost/test/unit_test.hpp"
#include "boost/asio.hpp"
using boost::asio::post;

#include <fcntl.h>

#if !BOOST_OS_WINDOWS
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "utils.hpp"
#include "PerformanceUtils.hpp"
#include "NewlineParser.hpp"
#include "FileReaders.hpp"

using parseContentFunc = utils::Lines( const char* data, const size_t size );

//! POSIX API with custom parseContent function
utils::FileView fromFileParser( const sys_string& filename, parseContentFunc parse ) {
    utils::FileView view;
    int file = open( filename.c_str(), O_RDONLY | O_BINARY );
    IF_RET( file == -1 );
    utils::ScopeGuard onExit( [file] { close( file ); } );

    view.size = utils::fileSize( file );
    IF_RET( !view.size );

    // growing buffer for each thread
    static thread_local utils::Buffer buffer;
    char* ptr = buffer.grow( view.size );

    size_t bytes = _read( file, ptr, view.size );
    IF_RET( view.size != bytes );

    // check first 100 bytes for binary
    IF_RET( !utils::isTextFile( std::string_view( ptr, std::min<size_t>( view.size, 100ul ) ) ) );

    view.content = std::string_view( ptr, view.size );
    view.lines = parse( ptr, view.size );
    return view;
}

utils::FileView fromFileUtils( const sys_string& filename ) {
    return fromFileParser( filename, []( const char* data, const size_t size ) { return utils::parseContent( data, size, size ); } );
}

utils::FileView fromFileForLoop( const sys_string& filename ) {
    return fromFileParser( filename, parseContentForLoop );
}

utils::FileView fromFileFind( const sys_string& filename ) {
    return fromFileParser( filename, parseContentFind );
}

using fromFileFunc = utils::FileView( const sys_string& filename );

std::map<fromFileFunc*, const char*> names = {
    {fromFileMmap, "fromFileMmap"},
    {fromFileLocal, "fromFileLocal"},
    {fromFileString, "fromFileString"},
    {fromFileLSeek, "fromFileLSeek"},
    {fromFileTwoFread, "fromFileTwoFread"},
    {fromFileCPP, "fromFileCPP"},
    {fromFileUtils, "fromFileUtils"},
    {fromFileForLoop, "fromFileForLoop"},
    {fromFileFind, "fromFileFind"},
    {fromFileCPP, "fromFileCPP"},
    {utils::fromFileP, "utils::fromFileC"},
#if BOOST_OS_WINDOWS
    {utils::fromWinAPI, "utils::fromWinAPI"},
#endif
};

long run( fromFileFunc fromFile ) {
    size_t sum = 0;
    size_t lineCount = 0;
    size_t files = 0;

    //! \note Windows is slow in I/O
#if BOOST_OS_WINDOWS
    fs::path include = "../../../../libs/boost/include/boost/asio";
#else
    fs::path include = "../../../../libs/boost";
#endif

    long ns = 0;
    STOPWATCH
    START

    utils::recurseDir( include.native(), [&sum, &lineCount, &files, fromFile]( const sys_string & filename ) {
        auto view = fromFile( filename );
        files++;
        sum += view.size;
        lineCount += view.lines.size();
    } );

    STOP( ns );

    printf( "%17s : %zu files, %5zu kB and %zu lines in %ld ms\n",
            names[fromFile], files, sum / 1024, lineCount, ns / 1000000 );
    return ns / 1000000;
}

// test file I/O
BOOST_AUTO_TEST_CASE( Test_fromFile ) {
    printf( "I/O\n" );
    size_t t2 = run( fromFileCPP );
    /*long tS = */run( fromFileString );
    /*long tL = */run( fromFileLocal );
    /*long tF = */run( fromFileLSeek );
    /*long tM = */run( fromFileMmap );
    /*long tO = */run( fromFileTwoFread );
#if BOOST_OS_WINDOWS
    /*long tW = */run( utils::fromWinAPI );
#endif
    long t1 = run( utils::fromFileP );
    printf( "\n" );
    BOOST_CHECK_LT( t1, t2 ); // assume FILE* is faster than std::ifstream
}

// test newline parsing
BOOST_AUTO_TEST_CASE( Test_parseContent ) {
    printf( "Newline\n" );
    long t2 = run( fromFileForLoop );
    long t1 = run( fromFileUtils );
    long tf = run( fromFileFind );
    printf( "\n" );
    BOOST_CHECK_LT( t1, t2 );
}

// test printing
BOOST_AUTO_TEST_CASE( Test_printf ) {
    printf( "Output\n" );

    std::string text = "text123";

#if BOOST_OS_WINDOWS
    FILE* file = fopen( L"dump.txt", L"w" );
#else
    FILE* file = fopen( "dump.txt", "w" );
#endif

    auto reset = [file] { fseek( file, 0, SEEK_SET ); };

    /*long t_write = */timed1000( "write", [file, text] {
        std::string data = "[" + text + "]\n";
        write( fileno( file ), data.c_str(), data.size() );
    }, reset );

    long t_printf = timed1000( "fprintf", [file, text] {
        fprintf( file, "%s%s]\n", "[", text.c_str() );
    }, reset );

    /*long t_fputs = */timed1000( "fputs", [file, text] {
        fputs( ( "[" + text + "]\n" ).c_str(), file );
    }, reset );

#ifdef __linux__
    /* long t_fputs_unlocked = */timed1000( "fputs_unlocked", [file, text] {
        fputs_unlocked( ( "[" + text + "]\n" ).c_str(), file );
    }, reset );
#endif

    long t_fwrite = timed1000( "fwrite", [file, text] {
        std::string data = "[" + text + "]\n";
        fwrite( data.c_str(), 1, data.size(), file );
    }, reset );

#ifdef __linux__
    /*long t_fwrite_unlocked = */timed1000( "fwrite_unlocked", [file, text] {
        std::string data = "[" + text + "]\n";
        fwrite_unlocked( data.c_str(), 1, data.size(), file );
    }, reset );
#endif

    fclose( file );
    BOOST_CHECK_LT( t_fwrite, t_printf ); // assume fwrite is faster than printf
    printf( "\n" );
}
