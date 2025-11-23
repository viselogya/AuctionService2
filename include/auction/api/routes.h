#pragma once

#include <vector>

#include <httplib.h>

#include "auction/core/auth_service.h"
#include "auction/core/service_registry.h"
#include "auction/service/lot_service.h"

namespace auction::api {

std::vector<core::ApiMethod> registerRoutes(httplib::Server& server, service::LotService& lotService,
                                            core::AuthService& authService);

}  // namespace auction::api

