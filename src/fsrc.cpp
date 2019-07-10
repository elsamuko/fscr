#include "threadpool.hpp"
#include "searcher.hpp"
#include "prettyprinter.hpp"
#include "stopwatch.hpp"
#include "utils.hpp"

void onAllFiles( Searcher& searcher ) {
    POOL;
    STOPWATCH
    START

    utils::recurseDir( searcher.opts.path.native(), [&pool, &searcher]( const sys_string & filename ) {
        pool.add( [filename, &searcher] {
            searcher.stats.filesSearched++;
            searcher.search( filename );
        } );
    } );

    STOP( searcher.stats.t_recurse )
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

    StopWatch total;
    total.start();

    SearchOptions opts = SearchOptions::parseArgs( argc, argv );

    if( !opts ) { return EXIT_FAILURE; }

    if( !fs::exists( opts.path ) ) {
        printf( "Dir \"%s\" does not exist.\n", opts.path.string().c_str() );
        exit( -1 );
    }

    bool colorized = opts.colorized;
    Color gray = colorized ? Color::Gray : Color::Neutral;

    auto makePrinter = [colorized] {
        PrettyPrinter* printer = new PrettyPrinter();
        printer->opts.colorized = colorized;
        return printer;
    };

    Searcher searcher( opts, makePrinter );

    if( !opts.noGit && fs::exists( opts.path / ".git" ) ) {
#ifdef _WIN32
        std::string nullDevice = "NUL";
#else
        std::string nullDevice = "/dev/null";
#endif
        fs::current_path( opts.path );
        STOPWATCH
        START
        // -c all from repo
        // -o other not ignored
        std::vector<sys_string> gitFiles = utils::run( "git ls-files -co --exclude-standard 2> " + nullDevice );
        STOP( searcher.stats.t_recurse );

        utils::printColor( gray, utils::format( "Searching for \"%s\" in repo:\n\n", searcher.opts.term.c_str() ) );
        onGitFiles( gitFiles, searcher );
    } else {
        utils::printColor( gray, utils::format( "Searching for \"%s\" in folder:\n\n", searcher.opts.term.c_str() ) );
        onAllFiles( searcher );
    }

    auto ms = total.stop() / 1000000;

#if DETAILED_STATS
    utils::printColor( gray, utils::format(
                           "Times: Recurse %ld ms, Read %ld ms, Search %ld ms, Collect %ld ms, Print %ld ms\n",
                           searcher.stats.t_recurse / 1000000,
                           searcher.stats.t_read / 1000000,
                           searcher.stats.t_search / 1000000,
                           searcher.stats.t_collect / 1000000,
                           searcher.stats.t_print / 1000000 ) );
#endif

    utils::printColor( gray, utils::format(
                           "Found %lu matches in %lu/%lu files (%lu kB) in %ld ms\n",
                           searcher.stats.matches.load(),
                           searcher.stats.filesMatched.load(),
                           searcher.stats.filesSearched.load(),
                           searcher.stats.bytesRead.load() / 1024,
                           ms ) );

    return 0;
}
