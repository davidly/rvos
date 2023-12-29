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

        size_t binary_search( uint64_t key )
        {
            if ( 0 == entries.size() )
               return (size_t) -1;
            
            size_t low = 0;
            size_t high = entries.size() - 1;
            while ( low <= high )
            {
                size_t mid = low + ( ( high - low ) / 2 );
                if ( entries[ mid ].address == key )
                    return mid;
                if ( key < entries[ mid ].address )
                    high = mid - 1;
                else
                    low = mid + 1;
            }
            return (size_t) -1;
        } //binary_search

    public:
        CMMap() : base(0), length(0) {}
        ~CMMap() {}

        void trace_allocations()
        {
            if ( entries.size() )
            {
                tracer.Trace( "app has %zu mmap allocations. address, size:\n", entries.size() );
                uint64_t total = 0;
                for ( size_t i = 0; i < entries.size(); i++ )
                {
                    tracer.Trace( "    %zu: %llx, %llu\n", i, entries[i].address, entries[i].length );
                    total += entries[i].length;
                }
                tracer.Trace( "  total memory in use: %llu bytes\n", total );
            }
        } //trace_allocations

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
                entries.push_back( entry );
                return base;
            }
            else
            {
                // to do: look for gaps that can be reused

                tracer.Trace( "  adding a subsequent mmap entry\n" );

                MMapEntry & entry = entries[ entries.size() - 1 ];
                uint64_t free_offset = entry.address + entry.length;

                if ( l < ( length - ( free_offset - base ) ) )
                {
                    MMapEntry newentry = { free_offset, l };
                    entries.push_back( newentry );
                    return free_offset;
                }
            }

            return 0;
        } //allocate

        bool free( uint64_t a, uint64_t l )
        {
            size_t match = binary_search( a );
            if ( -1 != match )
            {
                if ( l < entries[ match ].length )
                    entries[ match ].length = l;
                else
                    entries.erase( entries.begin() + match );
            }
            else
            {
                tracer.Trace( "  munmap can't find entry %llu to free\n", a );
                return false;
            }

            return true;
        } //free
};


