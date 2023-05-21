#include "../common_impl.hpp"
#include <tephra/utils/standard_report_handler.hpp>
#include <debug-trap.h>
#include <sstream>

namespace tp {
namespace utils {

    void formatDebugContext(std::ostringstream& msgStream, const tp::DebugMessageContext& context) {
        msgStream << "[";
        if (context.parentObjectName != nullptr)
            msgStream << "'" << context.parentObjectName << "'->";

        if (context.objectName != nullptr)
            msgStream << "'" << context.objectName << "'->";
        else if (context.typeName != nullptr)
            msgStream << context.typeName << "->";

        msgStream << context.methodName << "(";

        if (context.parameter != nullptr)
            msgStream << context.parameter;

        msgStream << ")] ";
    }

    StandardReportHandler::StandardReportHandler(
        std::ostream& outStream,
        tp::DebugMessageSeverityMask severityMask,
        tp::DebugMessageTypeMask typeMask,
        tp::DebugMessageSeverityMask trapSeverityMask)
        : outStream(outStream),
          severityMask(severityMask),
          typeMask(typeMask),
          trapSeverityMask(trapSeverityMask),
          seenSeveritiesMask(tp::DebugMessageSeverityMask::None()) {}

    void StandardReportHandler::callbackMessage(const tp::DebugMessage& message) noexcept {
        switch (message.severity) {
        case tp::DebugMessageSeverity::Verbose:
            outStream << "VERBOSE ";
            break;
        case tp::DebugMessageSeverity::Information:
            outStream << "INFO ";
            break;
        case tp::DebugMessageSeverity::Warning:
            outStream << "WARNING ";
            break;
        case tp::DebugMessageSeverity::Error:
            outStream << "! ERROR ";
            break;
        }

        std::string formattedMessage = formatDebugMessage(message);
        outStream << formattedMessage << std::endl;

        seenSeveritiesMask |= message.severity;

        if (trapSeverityMask.contains(message.severity)) {
            triggerDebugTrap();
        }
    }

    void StandardReportHandler::callbackRuntimeError(
        const tp::DebugMessageContext& context,
        const tp::RuntimeError& error) noexcept {
        std::string formattedError = formatRuntimeError(context, error);
        outStream << "! ERROR THROWN " << formattedError << std::endl;
    }

    std::string StandardReportHandler::formatDebugMessage(const tp::DebugMessage& message) noexcept {
        std::ostringstream msgStream;

        switch (message.type) {
        case tp::DebugMessageType::Performance:
            msgStream << "Performance ";
            break;
        case tp::DebugMessageType::Validation:
            msgStream << "Validation ";
            break;
        }

        if (message.context.methodName != nullptr) {
            formatDebugContext(msgStream, message.context);
        }

        if (message.vkCallbackData != nullptr) {
            msgStream << "<";
            msgStream << message.vkCallbackData->messageIdNumber << "|";
            if (message.vkCallbackData->pMessageIdName != nullptr) {
                msgStream << message.vkCallbackData->pMessageIdName << "> ";
            } else {
                msgStream << "N/A> ";
            }
        }

        if (message.message != nullptr) {
            msgStream << ": " << message.message;
        } else {
            msgStream << ": (message missing)";
        }

        if (message.vkCallbackData != nullptr) {
            bool addedHeader = false;
            for (uint32_t i = 0; i < message.vkCallbackData->objectCount; i++) {
                const VkDebugUtilsObjectNameInfoEXT* object = &message.vkCallbackData->pObjects[i];
                if (object->pObjectName != nullptr) {
                    if (!addedHeader) {
                        msgStream << "\n    Named objects: " << object->pObjectName;
                        addedHeader = true;
                    } else {
                        msgStream << ", " << object->pObjectName;
                    }
                }
            }

            addedHeader = false;
            for (uint32_t i = 0; i < message.vkCallbackData->cmdBufLabelCount; i++) {
                const VkDebugUtilsLabelEXT* object = &message.vkCallbackData->pCmdBufLabels[i];
                if (object->pLabelName != nullptr) {
                    if (!addedHeader) {
                        msgStream << "\n    Cmd buffer labels: " << object->pLabelName;
                        addedHeader = true;
                    } else {
                        msgStream << ", " << object->pLabelName;
                    }
                }
            }
        }

        return msgStream.str();
    }

    std::string StandardReportHandler::formatRuntimeError(
        const tp::DebugMessageContext& context,
        const tp::RuntimeError& error) noexcept {
        std::ostringstream msgStream;

        if (context.methodName != nullptr) {
            formatDebugContext(msgStream, context);
        }

        msgStream << ": " << tp::RuntimeError::getErrorTypeDescription(error.getErrorType());

        if (error.what() != nullptr) {
            msgStream << " - " << error.what();
        }

        return msgStream.str();
    }

    void StandardReportHandler::triggerDebugTrap() noexcept {
        psnip_trap();
    }

}
}
