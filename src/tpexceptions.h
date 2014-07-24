#ifndef TAILPRODUCE_EXCEPTIONS_H
#define TAILPRODUCE_EXCEPTIONS_H

// TODO(dkorolev): Add descriptions to each exception, perhaps as part of their respective what()-s.

namespace TailProduce {
    // Exception types.
    struct Exception : std::exception {
        std::string text;
        explicit Exception(const std::string& text = "TailProduce Exception") : text(text) {
        }
        virtual const char* what() const noexcept {
            return text.c_str();
        }
    };
    struct InternalError : Exception {};
    struct StorageException : Exception {};
    struct StorageEmptyKeyException : StorageException {};
    struct StorageEmptyValueException : StorageException {};
    struct StorageNoDataException : StorageException {};
    struct StorageOverwriteNotAllowedException : StorageException {};
    struct StorageIteratorOutOfBoundsException : StorageException {};
    struct OrderKeysGoBackwardsException : Exception {};
    struct ListenerHasNoDataToRead : Exception {};
    struct AttemptedToAdvanceListenerWithNoDataAvailable : Exception {};
    struct StreamDoesNotExistException : Exception {};
    struct MalformedStorageHeadException : Exception {};
    struct StreamAlreadyListedForCreationException : Exception {};
    struct StreamAlreadyExistsException : Exception {};
    struct StreamHasNoWriterDefinedException : Exception {
        explicit StreamHasNoWriterDefinedException(const std::string& name)
            : Exception("StreamHasNoWriterDefinedException: '" + name + "'.") {
        }
    };
};

#endif
