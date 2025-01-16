#pragma once

#include <opio/proto_entry/entry_base.hpp>

namespace opio::proto_entry
{

//
// std_entry_shortcuts_factory
//

template < template < class, class > class Entry_Type, typename Stats_Driver >
struct std_entry_shortcuts_factory
{
    template < typename Message_Consumer,
               typename Logger,
               protobuf_parsing_strategy Protobuf_Parsing_Strategy =
                   protobuf_parsing_strategy::trivial >
    using singlethread_t =
        Entry_Type< ::opio::proto_entry::singlethread_traits_base_t<
                        Stats_Driver,
                        Logger,
                        Protobuf_Parsing_Strategy >,
                    Message_Consumer >;

    template < typename Message_Consumer,
               typename Logger,
               protobuf_parsing_strategy Protobuf_Parsing_Strategy =
                   protobuf_parsing_strategy::trivial >
    using multithread_t =
        Entry_Type< ::opio::proto_entry::multithread_traits_base_t<
                        Stats_Driver,
                        Logger,
                        Protobuf_Parsing_Strategy >,

                    Message_Consumer >;
};

}  // namespace opio::proto_entry
