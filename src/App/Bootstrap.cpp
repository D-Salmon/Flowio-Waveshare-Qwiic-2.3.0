#include "App/Bootstrap.h"

#include "App/BuildFlags.h"
#include "Profiles/FlowIOS3/FlowIOS3Profile.h"

namespace {

AppContext gContext{};
bool gStarted = false;

const FirmwareProfile& resolveProfile()
{
    return Profiles::FlowIOS3::profile();
}

}  // namespace

namespace Bootstrap {

void run()
{
    if (gStarted) return;

    const FirmwareProfile& profile = resolveProfile();
    gContext.profile = &profile;
    gContext.board = profile.board;
    gContext.domain = profile.domain;
    gContext.identity = &profile.identity;
    gContext.supervisorRuntime = profile.supervisorRuntime;

    if (profile.setup) {
        profile.setup(gContext);
    }

    gContext.bootCompleted = true;
    gStarted = true;
}

void loop()
{
    if (!gStarted) {
        run();
    }

    (void)gContext.moduleManager.tickStartup(gContext.registry, gContext.services);

    if (gContext.profile && gContext.profile->loop) {
        gContext.profile->loop(gContext);
    }
}

AppContext& context()
{
    return gContext;
}

const FirmwareProfile& activeProfile()
{
    return resolveProfile();
}

}  // namespace Bootstrap
