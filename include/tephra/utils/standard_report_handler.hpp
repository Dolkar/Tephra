#pragma once

#include <tephra/tephra.hpp>
#include <iostream>

namespace tp {
namespace utils {

    /// A tp::DebugReportHandler implementation with default message formatting, writing to a standard output stream.
    class StandardReportHandler : public tp::DebugReportHandler {
    public:
        /// @param outStream
        ///     The output stream that messages and errors will be reported to.
        /// @param severityMask
        ///     The mask of severity levels to be reported.
        /// @param typeMask
        ///     The mask of message types to be reported.
        /// @param trapSeverityMask
        ///     The mask of severity levels that will attempt to trigger a debug trap (breakpoint) if a debugger is
        ///     attached. The message must be reported to do so, therefore severityMask and typeMask also need to be
        ///     satisfied. Does not work with all debuggers.
        StandardReportHandler(
            std::ostream& outStream = std::cerr,
            tp::DebugMessageSeverityMask severityMask = tp::DebugMessageSeverity::Warning |
                tp::DebugMessageSeverity::Error,
            tp::DebugMessageTypeMask typeMask = tp::DebugMessageType::General | tp::DebugMessageType::Validation |
                tp::DebugMessageType::Performance,
            DebugMessageSeverityMask trapSeverityMask = tp::DebugMessageSeverity::Error);

        virtual void callbackMessage(const tp::DebugMessage& message) noexcept override;

        virtual void callbackRuntimeError(
            const tp::DebugMessageContext& context,
            const tp::RuntimeError& error) noexcept override;

        virtual tp::DebugMessageSeverityMask getSeverityMask() const noexcept override {
            return severityMask;
        }

        virtual tp::DebugMessageTypeMask getTypeMask() const noexcept override {
            return typeMask;
        }

        /// Returns a mask of severities of all messages that have been logged so far.
        ///
        /// This can be useful to terminate an application after a validation error has been observed.
        tp::DebugMessageSeverityMask getSeenSeverities() const {
            return seenSeveritiesMask;
        }

        /// Clears the mask of seen severities.
        void clearSeenSeverities() {
            seenSeveritiesMask = tp::DebugMessageSeverityMask::None();
        }

        virtual ~StandardReportHandler() = default;

        /// Formats the debug message as a string in the default way, without the severity prefix.
        /// @param message
        ///     The details of the message.
        static std::string formatDebugMessage(const tp::DebugMessage& message) noexcept;

        /// Formats the error as a string in the default way, without the prefix.
        /// @param context
        ///     The Tephra context of where the error was triggered.
        /// @param error
        ///     The runtime error thrown.
        static std::string formatRuntimeError(
            const tp::DebugMessageContext& context,
            const tp::RuntimeError& error) noexcept;

        /// Triggers a debug trap (breakpoint) in a multiplatform way if a debugger is attached.
        static void triggerDebugTrap() noexcept;

    protected:
        std::ostream& outStream;
        tp::DebugMessageSeverityMask severityMask;
        tp::DebugMessageTypeMask typeMask;
        tp::DebugMessageSeverityMask trapSeverityMask;
        // TODO: Make atomic?
        tp::DebugMessageSeverityMask seenSeveritiesMask;
    };

}
}
