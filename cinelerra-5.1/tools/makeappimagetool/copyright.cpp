// local includes
#include "includes/log.h"
#include "includes/util.h"
#include "includes/copyright.h"

// specializations
#include "includes/copyright_dpkgquery.h"

namespace linuxdeploy {
    namespace core {
        namespace copyright {
            using namespace log;

            std::shared_ptr<ICopyrightFilesManager> ICopyrightFilesManager::getInstance() {
                if (!util::which("dpkg-query").empty()) {
                    ldLog() << LD_DEBUG << "Using dpkg-query to search for copyright files" << std::endl;
                    return std::make_shared<DpkgQueryCopyrightFilesManager>();
                }

                ldLog() << LD_DEBUG << "No usable copyright files manager implementation found" << std::endl;
                return nullptr;
            }
        }
    }
}
