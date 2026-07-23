#include "Profiles/FlowIOS3/FlowIOS3Profile.h"

#include "Board/BoardCatalog.h"
#include "Board/BoardSpec.h"

namespace Profiles {
namespace FlowIOS3 {

namespace {

int oneWirePinForSignal(const BoardSpec& board, BoardSignal signal, int fallbackPin)
{
    const OneWireBusSpec* spec = boardFindOneWire(board, signal);
    return spec ? spec->pin : fallbackPin;
}

}  // namespace

ModuleInstances::ModuleInstances(const BoardSpec& board)
    : ethernetModule(board),
      wifiModule(board),
      webInterfaceModule(board),
      firmwareUpdateModule(board),
      ioModule(board),
      oneWireWater(oneWirePinForSignal(board, BoardSignal::TempProbe1, 47)),
      oneWireAir(oneWirePinForSignal(board, BoardSignal::TempProbe2, 48))
{
}

ModuleInstances& moduleInstances()
{
    static ModuleInstances instances{BoardCatalog::activeBoard()};
    return instances;
}

}  // namespace FlowIOS3
}  // namespace Profiles
