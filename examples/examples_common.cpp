#include "examples_common.hpp"

#include <string>
#include <cstdlib>

void showErrorAndExit(std::string errorType, std::string errorDetail) {
    std::string errorBody = errorDetail + "\nSee \"examples_log.txt\" for additional details.";
    std::cerr << errorType << ": " << errorBody << std::endl;

#ifdef VK_USE_PLATFORM_WIN32_KHR
    // On Windows also show a message box because stderr gets hidden
    // Just assume our errors have only single byte characters
    std::wstring wErrorType = std::wstring(errorType.begin(), errorType.end());
    std::wstring wErrorBody = std::wstring(errorBody.begin(), errorBody.end());
    MessageBox(nullptr, wErrorBody.data(), wErrorType.data(), MB_OK);
#endif

    std::abort();
}
