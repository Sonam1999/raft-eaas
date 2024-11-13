#include <cassert>
#include <getopt.h>
#include <iostream>
#include <LogCabin/Client.h>
#include <LogCabin/Debug.h>
#include <LogCabin/Util.h>

using namespace LogCabin::Client;

// Global variable declarations
Tree tree;
Cluster *clusterPtr = nullptr; // Pointer to Cluster for optional lazy initialization

namespace
{

    using LogCabin::Client::Util::parseNonNegativeDuration;

    // Parses argv for the main function.
    class OptionParser
    {
    public:
        OptionParser(int &argc, char **&argv)
            : argc(argc), argv(argv), cluster("logcabin:5254"), logPolicy(""), timeout(parseNonNegativeDuration("0s"))
        {
            while (true)
            {
                static struct option longOptions[] = {
                    {"cluster", required_argument, NULL, 'c'},
                    {"help", no_argument, NULL, 'h'},
                    {"timeout", required_argument, NULL, 't'},
                    {"verbose", no_argument, NULL, 'v'},
                    {"verbosity", required_argument, NULL, 256},
                    {0, 0, 0, 0}};
                int c = getopt_long(argc, argv, "c:t:hv", longOptions, NULL);

                if (c == -1)
                    break;

                switch (c)
                {
                case 'c':
                    cluster = optarg;
                    break;
                case 't':
                    timeout = parseNonNegativeDuration(optarg);
                    break;
                case 'h':
                    usage();
                    exit(0);
                case 'v':
                    logPolicy = "VERBOSE";
                    break;
                case 256:
                    logPolicy = optarg;
                    break;
                case '?':
                default:
                    usage();
                    exit(1);
                }
            }
        }

        void usage()
        {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl
                      << "Options:" << std::endl
                      << "  -c <addresses>, --cluster=<addresses>  "
                      << "Network addresses of the LogCabin servers"
                      << " [default: logcabin:5254]" << std::endl
                      << "  -h, --help                     Print this usage information" << std::endl
                      << "  -t <time>, --timeout=<time>    Set timeout for operations [default: 0s]" << std::endl
                      << "  -v, --verbose                  Same as --verbosity=VERBOSE" << std::endl
                      << "  --verbosity=<policy>           Set which log messages are shown." << std::endl;
        }

        int &argc;
        char **&argv;
        std::string cluster;
        std::string logPolicy;
        uint64_t timeout;
    };

    // Define helper functions using global `tree` object
    std::string Get()
    {
        return tree.readEx("/etc/passwd");
    }

    void Put()
    {
        tree.writeEx("/etc/passwd", "ha");
    }

    void StartSUT()
    {
        tree.makeDirectoryEx("/etc");
    }

} // anonymous namespace

int main(int argc, char **argv)
{
    try
    {
        // Parse options
        OptionParser options(argc, argv);
        LogCabin::Client::Debug::setLogPolicy(
            LogCabin::Client::Debug::logPolicyFromString(options.logPolicy));

        // Initialize cluster and tree with global variables
        clusterPtr = new Cluster(options.cluster);
        tree = clusterPtr->getTree();
        tree.setTimeout(options.timeout);

        // Perform operations
        StartSUT();            // Create directory
        Put();                 // Write data
        assert(Get() == "ha"); // Read and verify data
        tree.removeDirectoryEx("/etc");

        // Cleanup
        delete clusterPtr;
        return 0;
    }
    catch (const LogCabin::Client::Exception &e)
    {
        std::cerr << "Exiting due to LogCabin::Client::Exception: " << e.what() << std::endl;
        delete clusterPtr;
        exit(1);
    }
}