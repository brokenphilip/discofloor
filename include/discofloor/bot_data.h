#pragma once

namespace discofloor
{
    class bot;
    
    // File-based (JSON) data structure for storing global, user or guild-specific bot data
    class data : public discofloor::file::json<-1, ' '>
    {
        using bulbtils::file::base::save;
        using bulbtils::file::base::load;
        friend class discofloor::bot;
    };
}