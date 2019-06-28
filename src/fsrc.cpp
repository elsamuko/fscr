#include "threadpool.hpp"
#include "searcher.hpp"
#include "utils.hpp"

void onAllFiles( Searcher& searcher ) {
    POOL;

    utils::recurseDir( searcher.opts.path.native(), [&pool, &searcher]( const sys_string & filename ) {
        pool.add( [filename, &searcher] {
            searcher.stats.filesSearched++;
            searcher.search( filename );
        } );
    } );
}

void onGitFiles( const std::vector<sys_string>& filenames, Searcher& searcher ) {
    POOL;

    for( const sys_string& filename : filenames ) {
        pool.add( [filename, &searcher] {
            searcher.stats.filesSearched++;
            searcher.search( filename );
        } );
    }
}

int main( int argc, char* argv[] ) {

    auto tp = std::chrono::system_clock::now();

    SearchOptions opts = SearchOptions::parseArgs( argc, argv );

    if( !opts ) { return EXIT_FAILURE; }

    if( !fs::exists( opts.path ) ) {
        printf( "Dir \"%s\" does not exist.\n", opts.path.string().c_str() );
        exit( -1 );
    }

    Searcher searcher( opts );

    if( !opts.noGit && fs::exists( opts.path / ".git" ) ) {
#ifdef _WIN32
        std::string nullDevice = "NUL";
#else
        std::string nullDevice = "/dev/null";
#endif
        fs::current_path( opts.path );
        std::vector<sys_string> gitFiles = utils::run( "git ls-files 2> " + nullDevice );
        printf( "Searching for \"%s\" in repo:\n\n", searcher.opts.term.c_str() );
        onGitFiles( gitFiles, searcher );
    } else {
        printf( "Searching for \"%s\" in folder:\n\n", searcher.opts.term.c_str() );
        onAllFiles( searcher );
    }

    auto duration = std::chrono::system_clock::now() - tp;
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>( duration );
    printf( "Found %lu matches in %lu/%lu files in %ld ms\n",
            searcher.stats.matches.load(), searcher.stats.filesMatched.load(),
            searcher.stats.filesSearched.load(), ms.count() );

    return 0;
}
