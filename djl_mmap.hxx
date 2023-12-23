#pragma once

#include <stdint.h>

struct MMapEntry
{
    uint64_t address;
    uint64_t length;
};

class CMMap
{
    private:
        vector<MMapEntry> entries; // sorted by address
        uint64_t base;
        uint64_t length;

    public:
        CMMap() : base(0), length(0) {}
        ~CMMap() {}

        void initialize( uint64_t b, uint64_t l )
        {
            base = b;
            length = l;
        } //initialize

        uint64_t allocate( uint64_t l )
        {
            if ( 0 == entries.size() )
            {
                tracer.Trace( "  adding a first mmap entry, l %llu, length %llu\n", l, length );
                if ( l > length )
                {
                    tracer.Trace( "  mmap alloc request %llu larger than reserved size %llu\n", l, length );
                    return 0;
                }

                tracer.Trace( "  in mmap allocate, base %#lx\n", base );

                MMapEntry entry = { base, l };
                entries.emplace_back( entry );
                return base;
            }
            else
            {
                tracer.Trace( "  adding a subsequent mmap entry\n" );

                MMapEntry & entry = entries[ entries.size() - 1 ];
                uint64_t free_offset = entry.address + entry.length;

                if ( l < ( length - ( free_offset - base ) ) )
                {
                    MMapEntry newentry = { free_offset, l };
                    entries.emplace_back( newentry );
                    return free_offset;
                }
            }

            return 0;
        } //allocate

        bool free( uint64_t a, uint64_t l )
        {
            for ( size_t i = 0; i < entries.size(); i++ )
            {
                if ( a == entries[ i ].address )
                {
                    if ( l < entries[ i ].length )
                        entries[ i ].length = l;
                    else
                        entries.erase( entries.begin() + i );
                    return true;
                }
            }

            tracer.Trace( "  munmap can't find entry %llu to free\n", a );
            return false;
        } //free
};


