#include <iostream>
#include <sstream>

#include "threadpool.hpp"
#include "utils.hpp"

#define LOG( A ) std::cout << A << std::endl;

struct Searcher {
    mutable std::mutex m;
    std::string term;
    void search( const std::experimental::filesystem::path& path ) const {
        // search only in text files
        if( !utils::isTextFile( path ) ) { return; }

        const std::list<std::string> lines = utils::fromFile( path );

        std::stringstream ss;
        size_t i = 0;

        for( const std::string& line : lines ) {
            i++;

            size_t pos = line.find( term );

            if( pos != std::string::npos ) {
                ss << path << " L" << i << " " << line.substr( 0, pos );
#if WIN32
                ss << line.substr( pos, term.size() );
#else
                // highlight first hit on linux
                ss << "\033[1;31m" << line.substr( pos, term.size() ) << "\033[0m";
#endif
                ss << line.substr( pos  + term.size() );
                ss << std::endl;
            }
        }

        std::string res = ss.str();

        if( !res.empty() ) {
            m.lock();
            LOG( res );
            m.unlock();
        }
    }
};

void onAllFiles( const std::experimental::filesystem::path searchpath, const Searcher& searcher ) {

    if( !std::experimental::filesystem::exists( searchpath ) ) {
        LOG( searchpath << " does not exist" );
        return;
    }

    if( !std::experimental::filesystem::is_directory( searchpath ) ) {
        LOG( searchpath << " is not a dir" );
        return;
    }

    auto start = std::experimental::filesystem::recursive_directory_iterator( searchpath );
    auto end   = std::experimental::filesystem::recursive_directory_iterator();

    ThreadPool pool;

    while( start != end ) {

        std::experimental::filesystem::path path = start->path();

        if( path.string().find( ".git" ) != std::string::npos ) {
            start.disable_recursion_pending();
            start++;
            path = start->path();
        }

        pool.add( [path, &searcher] {
            searcher.search( path );
        } );
        start++;
    }

    pool.waitForAllJobs();
}

void onGitFiles( const std::list<std::string>& filenames, const Searcher& searcher ) {
    ThreadPool pool;

    for( const std::string& filename : filenames ) {
        std::experimental::filesystem::path path( filename );
        pool.add( [path, &searcher] {
            searcher.search( path );
        } );
    }

    pool.waitForAllJobs();
}

int main( int argc, char* argv[] ) {

    if( argc != 2 ) {
        LOG( "Usage: fscr <term>" );
        return 0;
    }

    std::experimental::filesystem::path searchpath = ".";
    const std::string term = argv[1];
    auto tp = std::chrono::system_clock::now();

    Searcher searcher;
    searcher.term = argv[1];

#if WIN32
    std::string nullDevice = "NUL";
#else
    std::string nullDevice = "/dev/null";
#endif
    std::list<std::string> gitFiles = utils::run( "git ls-files 2> " + nullDevice );

    if( !gitFiles.empty() ) {
        LOG( "Searching for \"" << term << "\" in repo:\n" );
        onGitFiles( gitFiles, searcher );
    } else {
        LOG( "Searching for \"" << term << "\" in folder:\n" );
        onAllFiles( searchpath, searcher );
    }

    auto duration = std::chrono::system_clock::now() - tp;
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>( duration );
    LOG( "Searched in " << ms.count() << " ms" );

    return 0;
}
