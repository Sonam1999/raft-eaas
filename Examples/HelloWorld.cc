#include <cassert>
#include <getopt.h>
#include <iostream>
#include <LogCabin/Client.h>
#include <LogCabin/Debug.h>
#include <LogCabin/Util.h>
#include "eaas/EaaS.h"

using namespace LogCabin::Client;
// Tree tree;
std::unique_ptr<Tree> tree;

namespace
{
    using LogCabin::Client::Cluster;

    using LogCabin::Client::Tree;
    using LogCabin::Client::Util::parseNonNegativeDuration;

    // Parses argv for the main function.
    class OptionParser
    {
    public:
        OptionParser(int &argc, char **&argv)
            : argc(argc), argv(argv), cluster("node0:26257"), logPolicy(""), timeout(parseNonNegativeDuration("0s"))
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

                // Detect the end of the options.
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
                    // getopt_long already printed an error message.
                    usage();
                    exit(1);
                }
            }
        }

        void usage()
        {
            std::cout
                << "Writes a value to LogCabin. This isn't very useful on its own "
                << "but serves as a"
                << std::endl
                << "good starting point for more sophisticated LogCabin client "
                << "programs."
                << std::endl
                << std::endl
                << "This program is subject to change (it is not part of "
                << "LogCabin's stable API)."
                << std::endl
                << std::endl

                << "Usage: " << argv[0] << " [options]"
                << std::endl
                << std::endl

                << "Options:"
                << std::endl

                << "  -c <addresses>, --cluster=<addresses>  "
                << "Network addresses of the LogCabin"
                << std::endl
                << "                                         "
                << "servers, comma-separated"
                << std::endl
                << "                                         "
                << "[default: logcabin:5254]"
                << std::endl

                << "  -h, --help                     "
                << "Print this usage information"
                << std::endl

                << "  -t <time>, --timeout=<time>    "
                << "Set timeout for individual operations"
                << std::endl
                << "                                 "
                << "(0 means wait forever) [default: 0s]"
                << std::endl

                << "  -v, --verbose                  "
                << "Same as --verbosity=VERBOSE"
                << std::endl

                << "  --verbosity=<policy>           "
                << "Set which log messages are shown."
                << std::endl
                << "                                 "
                << "Comma-separated LEVEL or PATTERN@LEVEL rules."
                << std::endl
                << "                                 "
                << "Levels: SILENT ERROR WARNING NOTICE VERBOSE."
                << std::endl
                << "                                 "
                << "Patterns match filename prefixes or suffixes."
                << std::endl
                << "                                 "
                << "Example: Client@NOTICE,Test.cc@SILENT,VERBOSE."
                << std::endl;
        }

        int &argc;
        char **&argv;
        std::string cluster;
        std::string logPolicy;
        uint64_t timeout;
    };
}

OptionParser *globalOptions = nullptr;

int Get(int64_t key, int columns[], int size, int results[])
{
    std::string keyString = std::to_string(key);

    for (int i = 0; i < size; i++)
    {
        std::string resultString = tree->readEx(keyString + "/" + std::to_string(columns[i]));
        try
        {
            results[i] = std::stoi(resultString);
        }
        catch (const std::invalid_argument &e)
        {
            std::cerr << "Invalid argument: Cannot convert " << resultString << " to an integer" << std::endl;
            return -1;
        }
        catch (const std::out_of_range &e)
        {
            std::cerr << "Out of range: " << resultString << " is too large to fit in an int" << std::endl;
            return -1;
        }
    }
    return 0; // Indicate success
}

int Put(int64_t key, int columns[], int values[], int size)
{
    std::string keyString = std::to_string(key);
    for (int i = 0; i < size; i++)
    {
        std::string val = std::to_string(values[i]);
        tree->writeEx(keyString + "/" + std::to_string(columns[i]), val);
    }
    return 0;
}

int CreateSchema(const std::uint64_t &key)
{
    std::string keyString = std::to_string(key);
    tree->makeDirectoryEx(keyString);
    return 0;
}

int Delete(const std::uint64_t &key)
{
    std::string keyString = std::to_string(key);
    tree->removeDirectoryEx(keyString);
    return 0;
}
void runRaft()
{
    Cluster cluster(globalOptions->cluster);
    tree = std::unique_ptr<Tree>(new Tree(cluster.getTree()));
    tree->setTimeout(globalOptions->timeout);
    std::uint64_t key = 1;
    int size = 5;

    int columns[size];
    int values[size];
    int results[size];

    for (int i = 0; i < size; i++)
    {
        columns[i] = i;           // columns = {0, 1, 2, 3, 4};
        values[i] = (i + 1) * 10; // values = {10, 20, 30, 40, 50};
        results[i] = 0;           // Initialize results to 0
    }

    // Perform operations
    CreateSchema(key);                // Create directory
    Put(key, columns, values, size);  // Write data
    Get(key, columns, size, results); // Read data

    for (int i = 0; i < size; i++)
    {
        // assert(results[i] == values[i]);
        if (results[i] != values[i])
        {
            std::cerr << "Mismatch at index " << i << ": expected " << values[i] << ", got " << results[i] << std::endl;
        }
    }

    // assert(Get(tree, key) == "ha"); // Read and verify data
    Delete(key); // Remove Directory
    std::cout << "All operations completed successfully!" << std::endl;
}

int main(int argc, char **argv)
{
    try
    {
        // Parse options
        OptionParser options(argc, argv);
        globalOptions = &options;
        LogCabin::Client::Debug::setLogPolicy(
            LogCabin::Client::Debug::logPolicyFromString(options.logPolicy));

        eaas::EaasInit();

        /* Register SUT Commands */
        eaas::EaasRegister(&Get, "get");
        eaas::EaasRegister(&Put, "put");
        eaas::EaasRegister(&CreateSchema, "db_init");
        // eaas::EaasRegister(&Drop, "db_teardown");
        eaas::EaasRegister(&Delete, "delete");
        // eaas::EaasRegister(&Set, "set");
        // eaas::EaasRegister(&Scan, "scan");
        // eaas::EaasRegister(&RevScan, "rev_scan");
        // eaas::EaasRegister(&BeginTransaction, "begin_tx");
        // eaas::EaasRegister(&CommitTransaction, "commit_tx");
        // eaas::EaasRegister(&RollbackTransaction, "rollback_tx");
        // eaas::EaasRegister(&BatchGet, "batch_get");
        // eaas::EaasRegister(&BatchPut, "batch_put");

        std::cout << "Running Raft" << std::endl;
        ;
        runRaft();
        eaas::EaasStartGRPC();
        return 0;
    }
    catch (const LogCabin::Client::Exception &e)
    {
        std::cerr << "Exiting due to LogCabin::Client::Exception: " << e.what() << std::endl;
        exit(1);
    }
}
