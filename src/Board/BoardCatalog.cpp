#include "Board/BoardCatalog.h"

#include "Board/FlowIODINBoards.h"

namespace BoardCatalog {

const BoardSpec& flowIOS3()
{
    return BoardProfiles::kFlowIOS3;
}

const BoardSpec& activeBoard()
{
    return flowIOS3();
}

}  // namespace BoardCatalog
