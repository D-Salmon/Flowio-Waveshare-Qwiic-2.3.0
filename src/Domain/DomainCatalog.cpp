#include "Domain/DomainCatalog.h"

#include "Domain/Pool/PoolDomain.h"

namespace DomainCatalog {

const DomainSpec& pool()
{
    return PoolDomain::kPoolDomain;
}

const DomainSpec& activeDomain()
{
    return pool();
}

}  // namespace DomainCatalog
