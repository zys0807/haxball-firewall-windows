/******************************************************************************
  PacketFilter.cpp - PacketFilter class implemenation.
 
                                                 Mahesh S
                                                 swatkat_thinkdigit@yahoo.co.in
                                                 http://swatrant.blogspot.com/

  Modified to allow real-time blocks without engine reinitialization.

******************************************************************************/

#include "PacketFilter.h"
#pragma comment(lib, "Fwpuclnt.lib")
#pragma comment(lib, "Rpcrt4.lib")

/******************************************************************************
PacketFilter::PacketFilter() - Constructor
*******************************************************************************/
PacketFilter::PacketFilter()
{
    try
    {
        // Initialize member variables.
        m_hEngineHandle = NULL;
        ::ZeroMemory( &m_subLayerGUID, sizeof( GUID ) );
    }
    catch(...)
    {
    }
}

/******************************************************************************
PacketFilter::~PacketFilter() - Destructor
*******************************************************************************/
PacketFilter::~PacketFilter()
{
    try
    {
        // Stop firewall before closing.
        StopFirewall();
    }
    catch(...)
    {
    }
}

/******************************************************************************
PacketFilter::CreateDeleteInterface - This method creates or deletes a packet
                                      filter interface.
*******************************************************************************/
DWORD PacketFilter::CreateDeleteInterface( bool bCreate )
{
    DWORD dwFwAPiRetCode = ERROR_BAD_COMMAND;
    try
    {
        if( bCreate )
        {
            // Create packet filter interface.
            dwFwAPiRetCode =  ::FwpmEngineOpen0( NULL,
                                                 RPC_C_AUTHN_WINNT,
                                                 NULL,
                                                 NULL,
                                                 &m_hEngineHandle );
        }
        else
        {
            if( NULL != m_hEngineHandle )
            {
                // Close packet filter interface.
                dwFwAPiRetCode = ::FwpmEngineClose0( m_hEngineHandle );
                m_hEngineHandle = NULL;
            }
        }
    }
    catch(...)
    {
    }
    return dwFwAPiRetCode;
}

/******************************************************************************
PacketFilter::BindUnbindInterface - This method binds to or unbinds from a
                                    packet filter interface.
*******************************************************************************/
DWORD PacketFilter::BindUnbindInterface( bool bBind )
{
    DWORD dwFwAPiRetCode = ERROR_BAD_COMMAND;
    try
    {
        if( bBind )
        {
            RPC_STATUS rpcStatus = {0};
            FWPM_SUBLAYER0 SubLayer = {0};

            // Create a GUID for our packet filter layer.
            rpcStatus = ::UuidCreate( &SubLayer.subLayerKey );
            if( NO_ERROR == rpcStatus )
            {
                // Save GUID.
                ::CopyMemory( &m_subLayerGUID,
                              &SubLayer.subLayerKey,
                              sizeof( SubLayer.subLayerKey ) );

                // Populate packet filter layer information.
                SubLayer.displayData.name = FIREWALL_SUBLAYER_NAMEW;
                SubLayer.displayData.description = FIREWALL_SUBLAYER_NAMEW;
                SubLayer.flags = 0;
                SubLayer.weight = 0x100;

                // Add packet filter to our interface.
                dwFwAPiRetCode = ::FwpmSubLayerAdd0( m_hEngineHandle,
                                                     &SubLayer,
                                                     NULL );
            }
        }
        else
        {
            // Delete packet filter layer from our interface.
            dwFwAPiRetCode = ::FwpmSubLayerDeleteByKey0( m_hEngineHandle,
                                                         &m_subLayerGUID );
            ::ZeroMemory( &m_subLayerGUID, sizeof( GUID ) );
        }
    }
    catch(...)
    {
    }
    return dwFwAPiRetCode;
}

/******************************************************************************
PacketFilter::ParseIPAddrString - This is an utility method to convert
                                  IP address in string format to byte array and
                                  hex formats.
*******************************************************************************/
bool PacketFilter::ParseIPAddrString( char* szIpAddr, UINT nStrLen, BYTE* pbHostOrdr, UINT nByteLen, ULONG& uHexAddr )
{
    bool bRet = true;
    try
    {
        UINT i = 0;
        UINT j = 0;
        UINT nPack = 0;
        char szTemp[2];

        // Build byte array format from string format.
        for( ; ( i < nStrLen ) && ( j < nByteLen ); )
        {
            if( '.' != szIpAddr[i] ) 
            {
                snprintf(szTemp, 2, "%c", szIpAddr[i] );
                nPack = (nPack*10) + ::atoi( szTemp );
            }
            else
            {
                pbHostOrdr[j] = nPack;
                nPack = 0;
                j++;
            }
            i++;
        }
        if( j < nByteLen )
        {
            pbHostOrdr[j] = nPack;

            // Build hex format from byte array format.
            for( j = 0; j < nByteLen; j++ )
            {
                uHexAddr = ( uHexAddr << 8 ) + pbHostOrdr[j];
            }
        }
    }
    catch(...)
    {
    }
    return bRet;
}

/******************************************************************************
PacketFilter::AddToBlockList - This public method allows caller to add
                               IP addresses which need to be blocked.
*******************************************************************************/
DWORD PacketFilter::Block( char* szIpAddrToBlock )
{
	DWORD dwFwAPiRetCode = ERROR_BAD_COMMAND;
    try
    {
        if( NULL != szIpAddrToBlock )
        {
            IPFILTERINFO stIPFilter = {0};

            // Get byte array format and hex format IP address from string format.
            ParseIPAddrString( szIpAddrToBlock,
                               strlen( szIpAddrToBlock ),
                               stIPFilter.bIpAddrToBlock,
                               BYTE_IPADDR_ARRLEN,
                               stIPFilter.uHexAddrToBlock );
			UINT32 ipVal = *((UINT32*)&stIPFilter.bIpAddrToBlock);

			if ((NULL != stIPFilter.bIpAddrToBlock) && (0 != stIPFilter.uHexAddrToBlock))
			{
				FWPM_FILTER0 Filter = { 0 };
				FWPM_FILTER_CONDITION0 Condition = { 0 };
				FWP_V4_ADDR_AND_MASK AddrMask = { 0 };

				// Prepare filter condition.
				Filter.subLayerKey = m_subLayerGUID;
				Filter.displayData.name = FIREWALL_SERVICE_NAMEW;
				Filter.layerKey = FWPM_LAYER_INBOUND_TRANSPORT_V4;
				Filter.action.type = FWP_ACTION_BLOCK;
				Filter.weight.type = FWP_EMPTY;
				Filter.filterCondition = &Condition;
				Filter.numFilterConditions = 1;

				// Remote IP address should match itFilters->uHexAddrToBlock.
				Condition.fieldKey = FWPM_CONDITION_IP_REMOTE_ADDRESS;
				Condition.matchType = FWP_MATCH_EQUAL;
				Condition.conditionValue.type = FWP_V4_ADDR_MASK;
				Condition.conditionValue.v4AddrMask = &AddrMask;

				// Add IP address to be blocked.
				AddrMask.addr = stIPFilter.uHexAddrToBlock;
				AddrMask.mask = VISTA_SUBNET_MASK;

				// Add filter condition to our interface. Save filter id in itFilters->u64VistaFilterId.
				dwFwAPiRetCode = FwpmFilterAdd0(m_hEngineHandle,
					&Filter,
					NULL,
					&(stIPFilter.u64VistaFilterId));
				filterIds.insert(std::make_pair(ipVal, stIPFilter.u64VistaFilterId));
			}
        }
    }
    catch(...)
    {
    }
	return dwFwAPiRetCode;
}


DWORD PacketFilter::Unblock(char* szIpAddrToBlock)
{
	DWORD dwFwAPiRetCode = ERROR_BAD_COMMAND;
	try
	{
		if (NULL != szIpAddrToBlock)
		{
			IPFILTERINFO stIPFilter = { 0 };

			// Get byte array format and hex format IP address from string format.
			ParseIPAddrString(szIpAddrToBlock,
				strlen(szIpAddrToBlock),
				stIPFilter.bIpAddrToBlock,
				BYTE_IPADDR_ARRLEN,
				stIPFilter.uHexAddrToBlock);
			UINT32 ipVal = *((UINT32*)&stIPFilter.bIpAddrToBlock);

			std::unordered_map<UINT32, UINT64>::iterator elm = filterIds.find(ipVal);
			if ((NULL != stIPFilter.bIpAddrToBlock) && (0 != stIPFilter.uHexAddrToBlock) && elm != filterIds.end())
			{
				// Delete all previously added filters.
				dwFwAPiRetCode = FwpmFilterDeleteById0(m_hEngineHandle,
					elm->second);
				filterIds.erase(elm);
			}
		}
	}
	catch (...)
	{
	}
	return dwFwAPiRetCode;
}


/******************************************************************************
PacketFilter::StartFirewall - This public method starts firewall.
*******************************************************************************/
BOOL PacketFilter::StartFirewall()
{
    BOOL bStarted = FALSE;
    try
    {
        // Create packet filter interface.
        if( ERROR_SUCCESS == CreateDeleteInterface( true ) )
        {
            // Bind to packet filter interface.
            if( ERROR_SUCCESS == BindUnbindInterface( true ) )
            {
                // Add filters.
                //AddRemoveFilter( true );

                bStarted = TRUE;
            }
        }
    }
    catch(...)
    {
    }
    return bStarted;
}

/******************************************************************************
PacketFilter::StopFirewall - This method stops firewall.
*******************************************************************************/
BOOL PacketFilter::StopFirewall()
{
    BOOL bStopped = FALSE;
    try
    {
		for (auto it = filterIds.begin(); it != filterIds.end(); it++)
		{
			FwpmFilterDeleteById0(m_hEngineHandle, it->second);
		}

        // Unbind from packet filter interface.
        if( ERROR_SUCCESS == BindUnbindInterface( false ) )
        {
            // Delete packet filter interface.
            if( ERROR_SUCCESS == CreateDeleteInterface( false ) )
            {
                bStopped = TRUE;
            }
        }
    }
    catch(...)
    {
    }
    return bStopped;
}