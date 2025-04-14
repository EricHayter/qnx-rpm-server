#ifndef QNX_UTILS_PROCESS_CONTROL_HPP
#define QNX_UTILS_PROCESS_CONTROL_HPP

#include <system_error>
#include <string>
#include <optional>
#include <vector>
#include <unistd.h>
#include <signal.h>

#ifdef __QNXNTO__
#include <sys/neutrino.h>
#include <sys/procfs.h>
#endif

// Forward declaration for ProcessInfo
namespace qnx
{
    namespace core
    {
        class ProcessInfo;
    }
}

namespace qnx
{
    namespace utils
    {

        /**
         * @class ProcessControl
         * @brief Utility class for process control operations
         */
        class ProcessControl
        {
        public:
            /**
             * @brief Send a signal to a process
             * @param pid Process ID
             * @param signal Signal number
             * @return true if successful, false otherwise
             */
            static bool sendSignal(pid_t pid, int signal);

            /**
             * @brief Suspend a process
             * @param pid Process ID
             * @return true if successful, false otherwise
             */
            static bool suspend(pid_t pid);

            /**
             * @brief Resume a process
             * @param pid Process ID
             * @return true if successful, false otherwise
             */
            static bool resume(pid_t pid);

            /**
             * @brief Terminate a process
             * @param pid Process ID
             * @return true if successful, false otherwise
             */
            static bool terminate(pid_t pid);

            /**
             * @brief Check if a process exists
             * @param pid Process ID
             * @return true if process exists, false otherwise
             */
            static bool exists(pid_t pid);

            /**
             * @brief Get the parent process ID
             * @param pid Process ID
             * @return Parent PID if available, nullopt otherwise
             */
            static std::optional<pid_t> getParentPid(pid_t pid);

            /**
             * @brief Get child processes
             * @param pid Process ID
             * @return Vector of child PIDs
             */
            static std::vector<pid_t> getChildProcesses(pid_t pid);

            /**
             * @brief Get process command line
             * @param pid Process ID
             * @return Command line string if available, empty string otherwise
             */
            static std::string getCommandLine(pid_t pid);

            /**
             * @brief Get process working directory
             * @param pid Process ID
             * @return Working directory path if available, empty string otherwise
             */
            static std::string getWorkingDirectory(pid_t pid);

            /**
             * @brief Get process information
             * @param pid Process ID
             * @return ProcessInfo structure if available, nullopt otherwise
             */
            static std::optional<struct ProcessInfo> getProcessInfo(pid_t pid);

        private:
            ProcessControl() = delete;
            ~ProcessControl() = delete;
            ProcessControl(const ProcessControl &) = delete;
            ProcessControl &operator=(const ProcessControl &) = delete;
        };

        /**
         * @struct ProcessInfo
         * @brief Simple structure for basic process information
         */
        struct ProcessInfo
        {
            double cpu_usage;
            size_t memory_usage;
        };

    } // namespace utils
} // namespace qnx

#endif /* QNX_UTILS_PROCESS_CONTROL_HPP */