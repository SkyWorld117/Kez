#include "advisor.h"

std::string advise(const std::string& abstract_pkg) {
    // This function is just a lookup table
    bool arm64 = getenv("FROMAGER_ARCH") == "arm64";

    if (abstract_pkg == "blas") {
        return arm64 ? "nvpl" : "intel-oneapi-mkl";
    } else if (abstract_pkg == "fftw3-api") {
        return arm64 ? "armpl" : "intel-oneapi-mkl";
    } else if (abstract_pkg == "lapack") {
        return arm64 ? "nvpl" : "intel-oneapi-mkl";
    } else if (abstract_pkg == "mpi") {
        return "openmpi";
    } else if (abstract_pkg == "scalapack") {
        return arm64 ? "nvpl" : "intel-oneapi-mkl";
    }

    ERROR("No advice available for abstract package: " + abstract_pkg);
    exit(EXIT_FAILURE);
}