// system includes
#include <glob.h>
#include <iostream>
#include <libgen.h>

// local includes
#include "includes/assert.h"
#include "includes/appdir.h"
#include "includes/args.hxx"
#include "includes/desktopfile.h"
#include "includes/elf_file.h"
#include "includes/log.h"
#include "includes/util.h"
#include "includes/core.h"

using namespace linuxdeploy;
using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;
using namespace linuxdeploy::util;
using namespace linuxdeploy::util::assert;

namespace bf = boost::filesystem;

bool isFile(const std::string& path) {
    return access(path.c_str(), F_OK) == 0;
}

std::string findAppimagetool() {
    std::vector<char> buf(PATH_MAX, '\0');

    if (readlink("/proc/self/exe", buf.data(), buf.size()) < 0) {
        return "";
    }

    auto currentDirPath = std::string(dirname(buf.data()));

    // search next to current binary
    std::vector<std::string> knownPaths = {
        currentDirPath + "/appimagetool",
        currentDirPath + "/appimagetool.AppImage",
        currentDirPath + "/appimagetool-x86_64.AppImage",
        currentDirPath + "/appimagetool-i686.AppImage",
    };

    std::stringstream ss;
    ss << getenv("PATH");

    // also search in PATH
    std::string currentPath;
    while (std::getline(ss, currentPath, ':')) {
        knownPaths.push_back(currentPath + "/appimagetool");
        knownPaths.push_back(currentPath + "/appimagetool.AppImage");
        knownPaths.push_back(currentPath + "/appimagetool-x86_64.AppImage");
        knownPaths.push_back(currentPath + "/appimagetool-i686.AppImage");
    }

    for (const auto& path : knownPaths) {
        if (isFile(path))
            return path;
    }

    return "";
}

int main(int argc, char** argv) {
    args::ArgumentParser parser(
        "makeappimage -- create AppDir bundles with ease"
    );

    args::HelpFlag help(parser, "help", "Display this help text", {'h', "help"});
    args::Flag showVersion(parser, "", "Print version and exit", {'V', "version"});
    args::ValueFlag<int> verbosity(parser, "verbosity", "Verbosity of log output (0 = debug, 1 = info (default), 2 = warning, 3 = error)", {'v', "verbosity"});

    args::ValueFlag<std::string> appDirPath(parser, "appdir", "Path to target AppDir", {"appdir"});

    args::ValueFlagList<std::string> sharedLibraryPaths(parser, "library", "Shared library to deploy", {'l', "library"});

    args::ValueFlagList<std::string> executablePaths(parser, "executable", "Executable to deploy", {'e', "executable"});

    args::ValueFlagList<std::string> deployDepsOnlyPaths(parser, "path", "Path to ELF file or directory containing such files (libraries or executables) in the AppDir whose dependencies shall be deployed by makeappimage without copying them into the AppDir", {"deploy-deps-only"});

    args::ValueFlagList<std::string> desktopFilePaths(parser, "desktop file", "Desktop file to deploy", {'d', "desktop-file"});
    args::Flag createDesktopFile(parser, "", "Create basic desktop file that is good enough for some tests", {"create-desktop-file"});

    args::ValueFlagList<std::string> iconPaths(parser, "icon file", "Icon to deploy", {'i', "icon-file"});
    args::ValueFlag<std::string> iconTargetFilename(parser, "filename", "Filename all icons passed via -i should be renamed to", {"icon-filename"});

    args::ValueFlag<std::string> customAppRunPath(parser, "AppRun path", "Path to custom AppRun script (makeappimage will not create a symlink but copy this file instead)", {"custom-apprun"});

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help&) {
        std::cerr << parser;

        // license information
        std::cerr << std::endl
                  << "===== library information =====" << std::endl
                  << std::endl
                  << "This software uses the great CImg library, as well as libjpeg and libpng as well as various Boost libraries." << std::endl
                  << std::endl
                  << "libjpeg license information: this software is based in part on the work of the Independent JPEG Group" << std::endl
                  << std::endl
                  << "CImg license information: This software is governed either by the CeCILL or the CeCILL-C "
                     "license under French law and abiding by the rules of distribution of free software. You can "
                     "use, modify and or redistribute the software under the terms of the CeCILL or CeCILL-C "
                     "licenses as circulated by CEA, CNRS and INRIA at the following URL: "
                     "\"http://cecill.info\"." << std::endl;

        return 0;
    } catch (args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    // always show version statement
    // TODO pick up from what is specified in configure.ac
    std::cerr << "MakeAppImage version " << "0.1.0" << std::endl;

    // if only the version should be shown, we can exit now
    if (showVersion)
        return 0;

    // set verbosity
    if (verbosity) {
        ldLog::setVerbosity((LD_LOGLEVEL) verbosity.Get());
    }

    if (!appDirPath) {
        ldLog() << LD_ERROR << "--appdir parameter required" << std::endl;
        std::cerr << std::endl << parser;
        return 1;
    }

    appdir::AppDir appDir(appDirPath.Get());

    // allow disabling copyright files deployment via environment variable
    if (getenv("DISABLE_COPYRIGHT_FILES_DEPLOYMENT") != nullptr) {
        ldLog() << std::endl << LD_WARNING << "Copyright files deployment disabled" << std::endl;
        appDir.setDisableCopyrightFilesDeployment(true);
    }

    // initialize AppDir with common directories
    ldLog() << std::endl << "-- Creating basic AppDir structure --" << std::endl;
    if (!appDir.createBasicStructure()) {
        ldLog() << LD_ERROR << "Failed to create basic AppDir structure" << std::endl;
        return 1;
    }

    ldLog() << std::endl << "-- Deploying dependencies for existing files in AppDir --" << std::endl;
    if (!appDir.deployDependenciesForExistingFiles()) {
        ldLog() << LD_ERROR << "Failed to deploy dependencies for existing files" << std::endl;
        return 1;
    }

    // deploy shared libraries to usr/lib, and deploy their dependencies to usr/lib
    if (sharedLibraryPaths) {
        ldLog() << std::endl << "-- Deploying shared libraries --" << std::endl;

        for (const auto& libraryPath : sharedLibraryPaths.Get()) {
            if (!bf::exists(libraryPath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << libraryPath << std::endl;
                return 1;
            }

            if (!appDir.forceDeployLibrary(libraryPath)) {
                ldLog() << LD_ERROR << "Failed to deploy library: " << libraryPath << std::endl;
                return 1;
            }
        }
    }

    // deploy executables to usr/bin, and deploy their dependencies to usr/lib
    if (executablePaths) {
        ldLog() << std::endl << "-- Deploying executables --" << std::endl;

        for (const auto& executablePath : executablePaths.Get()) {
            if (!bf::exists(executablePath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << executablePath << std::endl;
                return 1;
            }

            if (!appDir.deployExecutable(executablePath)) {
                ldLog() << LD_ERROR << "Failed to deploy executable: " << executablePath << std::endl;
                return 1;
            }
        }
    }

    // deploy executables to usr/bin, and deploy their dependencies to usr/lib
    if (deployDepsOnlyPaths) {
        ldLog() << std::endl << "-- Deploying dependencies only for ELF files --" << std::endl;

        for (const auto& path : deployDepsOnlyPaths.Get()) {
            if (bf::is_directory(path)) {
                ldLog() << "Deploying files in directory" << path << std::endl;

                for (auto it = bf::directory_iterator{path}; it != bf::directory_iterator{}; ++it) {
                    if (!bf::is_regular_file(*it)) {
                        continue;
                    }

                    if (!appDir.deployDependenciesOnlyForElfFile(*it, true)) {
                        ldLog() << LD_WARNING << "Failed to deploy dependencies for ELF file" << *it << LD_NO_SPACE << ", skipping" << std::endl;
                        continue;
                    }
                }
            } else if (bf::is_regular_file(path)) {
                if (!appDir.deployDependenciesOnlyForElfFile(path)) {
                    ldLog() << LD_ERROR << "Failed to deploy dependencies for ELF file: " << path << std::endl;
                    return 1;
                }
            } else {
                ldLog() << LD_ERROR << "No such file or directory: " << path << std::endl;
                return 1;
            }
        }
    }

    // perform deferred copy operations before running input plugins to make sure all files the plugins might expect
    // are in place
    ldLog() << std::endl << "-- Copying files into AppDir --" << std::endl;
    if (!appDir.executeDeferredOperations()) {
        return 1;
    }


    if (iconPaths) {
        ldLog() << std::endl << "-- Deploying icons --" << std::endl;

        for (const auto& iconPath : iconPaths.Get()) {
            if (!bf::exists(iconPath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << iconPath << std::endl;
                return 1;
            }

            bool iconDeployedSuccessfully;

            if (iconTargetFilename) {
                iconDeployedSuccessfully = appDir.deployIcon(iconPath, iconTargetFilename.Get());
            } else {
                iconDeployedSuccessfully = appDir.deployIcon(iconPath);
            }

            if (!iconDeployedSuccessfully) {
                ldLog() << LD_ERROR << "Failed to deploy icon: " << iconPath << std::endl;
                return 1;
            }
        }
    }

    if (desktopFilePaths) {
        ldLog() << std::endl << "-- Deploying desktop files --" << std::endl;

        for (const auto& desktopFilePath : desktopFilePaths.Get()) {
            if (!bf::exists(desktopFilePath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << desktopFilePath << std::endl;
                return 1;
            }

            desktopfile::DesktopFile desktopFile(desktopFilePath);

            if (!appDir.deployDesktopFile(desktopFile)) {
                ldLog() << LD_ERROR << "Failed to deploy desktop file: " << desktopFilePath << std::endl;
                return 1;
            }
        }
    }

    // perform deferred copy operations before creating other files here before trying to copy the files to the AppDir root
    ldLog() << std::endl << "-- Copying files into AppDir --" << std::endl;
    if (!appDir.executeDeferredOperations()) {
        return 1;
    }

    if (createDesktopFile) {
        if (!executablePaths) {
            ldLog() << LD_ERROR << "--create-desktop-file requires at least one executable to be passed" << std::endl;
            return 1;
        }

        ldLog() << std::endl << "-- Creating desktop file --" << std::endl;
        ldLog() << LD_WARNING << "Please beware the created desktop file is of low quality and should be edited or replaced before using it for production releases!" << std::endl;

        auto executableName = bf::path(executablePaths.Get().front()).filename().string();

        auto desktopFilePath = appDir.path() / "usr/share/applications" / (executableName + ".desktop");

        if (bf::exists(desktopFilePath)) {
            ldLog() << LD_WARNING << "Working on existing desktop file:" << desktopFilePath << std::endl;
        } else {
            ldLog() << "Creating new desktop file:" << desktopFilePath << std::endl;
        }

        desktopfile::DesktopFile desktopFile;
        if (!addDefaultKeys(desktopFile, executableName)) {
            ldLog() << LD_WARNING << "Tried to overwrite existing entries in desktop file:" << desktopFilePath << std::endl;
        }

        if (!desktopFile.save(desktopFilePath.string())) {
            ldLog() << LD_ERROR << "Failed to save desktop file:" << desktopFilePath << std::endl;
            return 1;
        }
    }

    if (!linuxdeploy::deployAppDirRootFiles(desktopFilePaths.Get(), customAppRunPath.Get(), appDir))
        return 1;

    // Find the external appimagetool and call it with the AppDir parameter.
    // This executable file or AppImage should be called "appimagetool".

    auto pathToAppimagetool = findAppimagetool();

    if (pathToAppimagetool.empty()) {
        std::cerr << "Could not find appimagetool in PATH" << std::endl;
        return 1;
    }

    std::cout << "Found appimagetool: " << pathToAppimagetool << std::endl;

    // Push program name to arg 0, and the appdir path to arg 1. 
    // The program name in arg 0 is always "appimagetool", even though
    // the actual executable name might be different.
    std::vector<char*> his_args;
    his_args.push_back(strdup("appimagetool"));
    his_args.push_back(strdup(appDirPath.Get().c_str()));
    his_args.push_back(nullptr);

    // This should put out two values.
    std::cerr << "Running command: " << pathToAppimagetool;
    for (auto it = his_args.begin() + 0; it != his_args.end(); it++) {
        std::cerr << " " << "\"" << *it << "\"";
    }

    std::cerr << std::endl;
    std::cerr << "execv called with " << pathToAppimagetool.c_str() << std::endl;
    std::cerr << std::endl << std::endl;

    // separate appimagetool output from my output
    std::cout << std::endl;

    execv(pathToAppimagetool.c_str(), his_args.data());

    auto error = errno;
    std::cerr << "execl() failed: " << strerror(error) << std::endl;

    return 0;
}

