# Patch staged OpENer source for NetBurner platform support.
param(
    [Parameter(Mandatory = $true)][string]$StageDir,
    [string]$Micro800CompatRoot = ''
)

$ErrorActionPreference = "Stop"

$cmakeLists = Join-Path $StageDir "CMakeLists.txt"
$generic = Join-Path $StageDir "src\ports\generic_networkhandler.c"

if (-not (Test-Path $cmakeLists)) { throw "Missing $cmakeLists" }
if (-not (Test-Path $generic)) { throw "Missing $generic" }

$cmake = Get-Content $cmakeLists -Raw
$cmake = $cmake -replace 'set\( OpENer_KNOWN_PLATFORMS "POSIX" "WIN32" "MINGW" "ClearCore" "ESP32" "RTEMS"\)',
    'set( OpENer_KNOWN_PLATFORMS "POSIX" "WIN32" "MINGW" "ClearCore" "ESP32" "RTEMS" "NETBURNER")'
if ($cmake -notmatch "NETBURNER") {
    $cmake = $cmake -replace '(if \(NOT \(\$\{OpENer_PLATFORM\} STREQUAL "RTEMS"\)\)\s*\r?\n\s*set\(OpENer_EXCLUDE_PATTERNS "\$\{OpENer_EXCLUDE_PATTERNS\} \*/src/ports/RTEMS/\*"\)\s*\r?\n\s*endif \(\))',
@'
if (NOT (${OpENer_PLATFORM} STREQUAL "RTEMS"))
        set(OpENer_EXCLUDE_PATTERNS "${OpENer_EXCLUDE_PATTERNS} */src/ports/RTEMS/*")
    endif ()
    if (NOT (${OpENer_PLATFORM} STREQUAL "NETBURNER"))
        set(OpENer_EXCLUDE_PATTERNS "${OpENer_EXCLUDE_PATTERNS} */src/ports/NETBURNER/*")
    endif ()
'@
}
Set-Content -Path $cmakeLists -Value $cmake -NoNewline

$src = Get-Content $generic -Raw

if ($src -notmatch "NetburnerOpenerNetworkInit") {
    $src = $src -replace '#define MAX_NO_OF_TCP_SOCKETS 10',
@'
#define MAX_NO_OF_TCP_SOCKETS 10

#ifdef OPENER_NETBURNER
extern EipStatus NetburnerOpenerNetworkInit(void);
extern int NetburnerCreateUdpIoSocket(void);
#endif
'@

    $oldBlock = @'
  /* create a new TCP socket */
  if( ( g_network_status.tcp_listener =
          (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) ) == -1 ) {
'@

    if ($src -notmatch [regex]::Escape($oldBlock.Trim())) {
        throw "generic_networkhandler.c socket block not found (init)"
    }

    $src = $src -replace '(?s)  /\* create a new TCP socket \*/.*?  highest_socket_handle = GetMaxSocket\(g_network_status\.tcp_listener,\s*\r?\n\s*g_network_status\.udp_global_broadcast_listener,\s*\r?\n\s*0,\s*\r?\n\s*g_network_status\.udp_unicast_listener\);',
@'
#ifdef OPENER_NETBURNER
  if (kEipStatusOk != NetburnerOpenerNetworkInit()) {
    OPENER_TRACE_ERR("networkhandler: NetburnerOpenerNetworkInit failed\n");
    goto network_init_error;
  }
#else
  /* create a new TCP socket */
  if( ( g_network_status.tcp_listener =
          (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error allocating socket, %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  int set_socket_option_value = 1; //Represents true for used set socket options
  /* Activates address reuse */
  if(setsockopt( g_network_status.tcp_listener, SOL_SOCKET, SO_REUSEADDR,
                 (char *) &set_socket_option_value,
                 sizeof(set_socket_option_value) ) == -1) {
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error setting socket option SO_REUSEADDR\n");
    goto network_init_error;
  }

  if(SetSocketToNonBlocking(g_network_status.tcp_listener) < 0) {
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error setting socket to non-blocking on new socket\n");
    goto network_init_error;
  }

  /* create a new UDP socket */
  if( ( g_network_status.udp_global_broadcast_listener =
          (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ) == kEipInvalidSocket ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error allocating socket, %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  /* create a new UDP socket */
  if( ( g_network_status.udp_unicast_listener =
          (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ) == kEipInvalidSocket ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error allocating socket, %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  /* Activates address reuse */
  set_socket_option_value = 1;
  if(setsockopt( g_network_status.udp_global_broadcast_listener, SOL_SOCKET,
                 SO_REUSEADDR, (char *) &set_socket_option_value,
                 sizeof(set_socket_option_value) )
     == -1) {
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error setting socket option SO_REUSEADDR\n");
    goto network_init_error;
  }

  if(SetSocketToNonBlocking(g_network_status.udp_global_broadcast_listener) <
     0) {
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error setting socket to non-blocking on new socket\n");
    goto network_init_error;
  }

  /* Activates address reuse */
  set_socket_option_value = 1;
  if(setsockopt( g_network_status.udp_unicast_listener, SOL_SOCKET,
                 SO_REUSEADDR,
                 (char *) &set_socket_option_value,
                 sizeof(set_socket_option_value) ) == -1) {
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error setting socket option SO_REUSEADDR\n");
    goto network_init_error;
  }

  if(SetSocketToNonBlocking(g_network_status.udp_unicast_listener) < 0) {
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error setting socket to non-blocking\n");
    goto network_init_error;
  }

  struct sockaddr_in my_address = {
    .sin_family = AF_INET,
    .sin_port = htons(kOpenerEthernetPort),
    .sin_addr.s_addr = g_network_status.ip_address
  };

  /* bind the new socket to port 0xAF12 (CIP) */
  if( ( bind( g_network_status.tcp_listener, (struct sockaddr *) &my_address,
              sizeof(struct sockaddr) ) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error with TCP bind: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  if( ( bind( g_network_status.udp_unicast_listener,
              (struct sockaddr *) &my_address,
              sizeof(struct sockaddr) ) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error with UDP bind: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  /* have QoS DSCP explicit appear on UDP responses to unicast messages */
  if(SetQosOnSocket( g_network_status.udp_unicast_listener,
                     CipQosGetDscpPriority(kConnectionObjectPriorityExplicit) )
     != 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_unicast_listener: error set QoS %d: %d - %s\n",
      g_network_status.udp_unicast_listener,
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    /* print message but don't abort by intent */
  }

  struct sockaddr_in global_broadcast_address = {
    .sin_family = AF_INET,
    .sin_port = htons(kOpenerEthernetPort),
    .sin_addr.s_addr = htonl(INADDR_ANY)
  };

  /* enable the UDP socket to receive broadcast messages */
  set_socket_option_value = 1;
  if( 0 >
      setsockopt( g_network_status.udp_global_broadcast_listener, SOL_SOCKET,
                  SO_BROADCAST, (char *) &set_socket_option_value,
                  sizeof(int) ) ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error with setting broadcast receive: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  if( ( bind( g_network_status.udp_global_broadcast_listener,
              (struct sockaddr *) &global_broadcast_address,
              sizeof(struct sockaddr) ) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error with UDP bind: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  /* have QoS DSCP explicit appear on UDP responses to broadcast messages */
  if(SetQosOnSocket( g_network_status.udp_global_broadcast_listener,
                     CipQosGetDscpPriority(kConnectionObjectPriorityExplicit) )
     != 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler udp_global_broadcast_listener: error set QoS %d: %d - %s\n",
      g_network_status.udp_global_broadcast_listener,
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    /* print message but don't abort by intent */
  }

  /* Make QoS DSCP explicit already appear on SYN connection establishment.
   * A newly accept()ed TCP socket inherits the setting from this socket.
   */
  if(SetQosOnSocket( g_network_status.tcp_listener,
                     CipQosGetDscpPriority(kConnectionObjectPriorityExplicit) )
     != 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error set QoS %d: %d - %s\n",
      g_network_status.tcp_listener,
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    /* print message but don't abort by intent */
  }

  /* switch socket in listen mode */
  if( ( listen(g_network_status.tcp_listener,
               MAX_NO_OF_TCP_SOCKETS) ) == -1 ) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR(
      "networkhandler tcp_listener: error with listen: %d - %s\n",
      error_code,
      error_message);
    FreeErrorMessage(error_message);
    goto network_init_error;
  }

  /* add the listener socket to the master set */
  FD_SET(g_network_status.tcp_listener, &master_socket);
  FD_SET(g_network_status.udp_unicast_listener, &master_socket);
  FD_SET(g_network_status.udp_global_broadcast_listener, &master_socket);

  /* keep track of the biggest file descriptor */
  highest_socket_handle = GetMaxSocket(g_network_status.tcp_listener,
                                       g_network_status.udp_global_broadcast_listener,
                                       0,
                                       g_network_status.udp_unicast_listener);
#endif
'@

    $src = $src -replace '(?s)  /\* create a new UDP socket \*/\s*\r?\n  g_network_status\.udp_io_messaging = \(int\)socket\(AF_INET, SOCK_DGRAM, IPPROTO_UDP\);.*?  if \(bind\( g_network_status\.udp_io_messaging, \(struct sockaddr \*\)&source_addr,\s*\r?\n            sizeof\(source_addr\) \) < 0\) \{.*?\r?\n    return kEipInvalidSocket;\s*\r?\n  \}',
@'
#ifdef OPENER_NETBURNER
  g_network_status.udp_io_messaging = NetburnerCreateUdpIoSocket();
  if (g_network_status.udp_io_messaging == kEipInvalidSocket) {
    OPENER_TRACE_ERR("networkhandler: cannot create UDP IO socket\n");
    return kEipInvalidSocket;
  }
#else
  /* create a new UDP socket */
  g_network_status.udp_io_messaging = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (g_network_status.udp_io_messaging == kEipInvalidSocket) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("networkhandler: cannot create UDP socket: %d- %s\n",
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
    return kEipInvalidSocket;
  }

  if (SetSocketToNonBlocking(g_network_status.udp_io_messaging) < 0) {
    OPENER_TRACE_ERR(
      "networkhandler udp_io_messaging: error setting socket to non-blocking on new socket\n");
    CloseUdpSocket(g_network_status.udp_io_messaging);
    g_network_status.udp_io_messaging = kEipInvalidSocket;
    OPENER_ASSERT(false);/* This should never happen! */
    return kEipInvalidSocket;
  }

  OPENER_TRACE_INFO("networkhandler: UDP socket %d\n",
                    g_network_status.udp_io_messaging);

  int option_value = 1;
  if (setsockopt( g_network_status.udp_io_messaging, SOL_SOCKET, SO_REUSEADDR,
                  (char *)&option_value, sizeof(option_value) ) < 0) {
    OPENER_TRACE_ERR(
      "error setting socket option SO_REUSEADDR on %s UDP socket\n");
    CloseUdpSocket(g_network_status.udp_io_messaging);
    g_network_status.udp_io_messaging = kEipInvalidSocket;
    return kEipInvalidSocket;
  }

  /* The bind on UDP sockets is necessary as the ENIP spec wants the source port to be specified to 2222 */
  struct sockaddr_in source_addr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_ANY),
    .sin_port = htons(kOpenerEipIoUdpPort)
  };

  if (bind( g_network_status.udp_io_messaging, (struct sockaddr *)&source_addr,
            sizeof(source_addr) ) < 0) {
    int error_code = GetSocketErrorNumber();
    char *error_message = GetErrorMessage(error_code);
    OPENER_TRACE_ERR("error on bind UDP for producing messages: %d - %s\n",
                     error_code,
                     error_message);
    FreeErrorMessage(error_message);
    CloseUdpSocket(g_network_status.udp_io_messaging);
    g_network_status.udp_io_messaging = kEipInvalidSocket;
    return kEipInvalidSocket;
  }
#endif
'@

    Set-Content -Path $generic -Value $src -NoNewline
}

$endian = Join-Path $StageDir "src\enet_encap\endianconv.c"
if (Test-Path $endian) {
    $esrc = Get-Content $endian -Raw
    if ($esrc -notmatch "OPENER_NETBURNER") {
        $esrc = $esrc -replace '#elif defined STM32\s*\r?\n#include "lwip/inet.h"\s*\r?\n#else\s*\r?\n#include <netinet/in.h>\s*\r?\n#include <sys/socket.h>',
@'
#elif defined STM32		 
#include "lwip/inet.h"
#elif defined(OPENER_NETBURNER)
#include "platform_network_includes.h"
#else
#include <netinet/in.h>
#include <sys/socket.h>
'@
    }

    if ($esrc -notmatch "NETBURNER_ENCADDR") {
        $esrc = $esrc -replace '(?s)void EncapsulateIpAddress\(EipUint16 port,\s*\r?\n\s*EipUint32 address,\s*\r?\n\s*ENIPMessage \*const outgoing_message\) \{.*?\n\}',
@'
void EncapsulateIpAddress(EipUint16 port,
                          EipUint32 address,
                          ENIPMessage *const outgoing_message) {
#if defined(OPENER_NETBURNER)
  /* NETBURNER_ENCADDR: ColdFire IPv4 is network-order in uint32; write high octet first. */
  AddIntToMessage(htons(AF_INET), outgoing_message);
  AddSintToMessage((unsigned char)((port >> 8) & 0xFFU), outgoing_message);
  AddSintToMessage((unsigned char)(port & 0xFFU), outgoing_message);
  AddSintToMessage((unsigned char)((address >> 24) & 0xFFU), outgoing_message);
  AddSintToMessage((unsigned char)((address >> 16) & 0xFFU), outgoing_message);
  AddSintToMessage((unsigned char)((address >> 8) & 0xFFU), outgoing_message);
  AddSintToMessage((unsigned char)(address & 0xFFU), outgoing_message);
#else
  if(kOpENerEndianessLittle == g_opener_platform_endianess) {
    AddIntToMessage(htons(AF_INET), outgoing_message);
    AddIntToMessage(port, outgoing_message);
    AddDintToMessage(address, outgoing_message);

  } else {
    if(kOpENerEndianessBig == g_opener_platform_endianess) {

      AddIntToMessage(htons(AF_INET), outgoing_message);

      AddSintToMessage( (unsigned char) (port >> 8), outgoing_message );
      AddSintToMessage( (unsigned char) port, outgoing_message );

      AddSintToMessage( (unsigned char) address, outgoing_message );
      AddSintToMessage( (unsigned char) (address >> 8), outgoing_message );
      AddSintToMessage( (unsigned char) (address >> 16), outgoing_message );
      AddSintToMessage( (unsigned char) (address >> 24), outgoing_message );

    } else {
      fprintf(stderr,
              "No endianess detected! Probably the DetermineEndianess function was not executed!");
      exit(EXIT_FAILURE);
    }
  }
#endif
}
'@
    }

    if ($esrc -notmatch "NETBURNER_ENDIAN_DETECT") {
        $esrc = $esrc -replace '(?s)void DetermineEndianess\(\) \{.*?\n\}',
@'
void DetermineEndianess() {
#if defined(OPENER_NETBURNER)
  /* NETBURNER_ENDIAN_DETECT */
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  g_opener_platform_endianess = kOpENerEndianessBig;
#else
  g_opener_platform_endianess = kOpENerEndianessLittle;
#endif
#else
  int i = 1;
  const char *const p = (char *) &i;
  if(p[0] == 1) {
    g_opener_platform_endianess = kOpENerEndianessLittle;
  } else {
    g_opener_platform_endianess = kOpENerEndianessBig;
  }
#endif
}
'@
    }

    Set-Content -Path $endian -Value $esrc -NoNewline
}

$cm = Join-Path $StageDir "src\cip\cipconnectionmanager.c"
$cmCmakePath = Join-Path $StageDir "src\cip\CMakeLists.txt"
if (Test-Path $cm) {
    $cmsrc = Get-Content $cm -Raw

    if ($cmsrc -notmatch "cipconnectionmanager_stats.h") {
        $cmsrc = $cmsrc -replace '#include "cipqos.h"',
            "#include `"cipqos.h`"`r`n#include `"cipconnectionmanager_stats.h`""
    }

    $cmsrc = $cmsrc -replace '0, /\* # of instance attributes \*/\s*\r?\n\s*14, /\* # highest instance attribute number\*/\s*\r?\n\s*8, /\* # of instance services \*/',
        "12, /* # of instance attributes */`r`n                                                14, /* # highest instance attribute number*/`r`n                                                9, /* # of instance services */"

    if ($cmsrc -notmatch "ConnectionManagerStatsInit") {
        $cmsrc = $cmsrc.Replace(
            "  InsertService(connection_manager,`r`n                kGetAttributeAll,`r`n                &GetAttributeAll,`r`n                `"GetAttributeAll`");",
            "  InsertService(connection_manager,`r`n                kGetAttributeAll,`r`n                &GetAttributeAll,`r`n                `"GetAttributeAll`");`r`n  InsertService(connection_manager,`r`n                kSetAttributeSingle,`r`n                &SetAttributeSingle,`r`n                `"SetAttributeSingle`");")
        $cmsrc = $cmsrc.Replace(
            "  InsertService(connection_manager,`r`n                kSearchConnectionData,`r`n                &SearchConnectionData,`r`n                `"SearchConnectionData`");",
            "  InsertService(connection_manager,`r`n                kSearchConnectionData,`r`n                &SearchConnectionData,`r`n                `"SearchConnectionData`");`r`n`r`n  ConnectionManagerStatsInit(connection_manager);")
    }

    if ($cmsrc -notmatch "ConnectionManagerStatsRecordOpenRequest") {
        $cmsrc = $cmsrc.Replace(
            "  (void) instance; /*suppress compiler warning */`r`n`r`n  bool is_null_request",
            "  (void) instance; /*suppress compiler warning */`r`n`r`n  ConnectionManagerStatsRecordOpenRequest();`r`n`r`n  bool is_null_request")
    }

    if ($cmsrc -notmatch "ConnectionManagerStatsRecordCloseRequest") {
        $cmsrc = $cmsrc.Replace(
            "  (void) encapsulation_session;`r`n`r`n  /* check connection_serial_number && originator_vendor_id && originator_serial_number if connection is established */",
            "  (void) encapsulation_session;`r`n`r`n  ConnectionManagerStatsRecordCloseRequest();`r`n`r`n  /* check connection_serial_number && originator_vendor_id && originator_serial_number if connection is established */")
    }

    [System.IO.File]::WriteAllText($cm, $cmsrc)
}

if (Test-Path $cmCmakePath) {
    $cmCmakeContent = Get-Content $cmCmakePath -Raw
    if ($cmCmakeContent -notmatch "cipconnectionmanager_stats.c") {
        $cmCmakeContent = $cmCmakeContent -replace 'cipconnectionmanager.c cipdlr.c',
            'cipconnectionmanager.c cipconnectionmanager_stats.c cipdlr.c'
        [System.IO.File]::WriteAllText($cmCmakePath, $cmCmakeContent)
    }
}

$tcpip = Join-Path $StageDir "src\cip\ciptcpipinterface.c"
if (Test-Path $tcpip) {
    $tcpipSrc = Get-Content $tcpip -Raw
    if ($tcpipSrc -notmatch "DecodeTcpIpInterfaceConfigurationWrapper") {
        $wrapperBlock = @'

static int DecodeTcpIpInterfaceConfigurationWrapper(void *const data,
		CipMessageRouterRequest *const message_router_request,
		CipMessageRouterResponse *const message_router_response) {
  return DecodeCipTcpIpInterfaceConfiguration(
            (CipTcpIpInterfaceConfiguration *)data,
            message_router_request,
            message_router_response);
}

static int DecodeTcpIpInterfaceHostNameWrapper(void *const data,
		CipMessageRouterRequest *const message_router_request,
		CipMessageRouterResponse *const message_router_response) {
  return DecodeCipTcpIpInterfaceHostName(
            (CipString *)data,
            message_router_request,
            message_router_response);
}

'@
        $tcpipSrc = $tcpipSrc -replace '(?s)(return number_of_decoded_bytes;\s*\r?\n\s*\}\s*\r?\n)#endif /\* defined \(OPENER_TCPIP_IFACE_CFG_SETTABLE\)',
            "`$1$wrapperBlock#endif /* defined (OPENER_TCPIP_IFACE_CFG_SETTABLE)"
        $tcpipSrc = $tcpipSrc.Replace(
            "DecodeCipTcpIpInterfaceConfiguration,`r`n                  &g_tcpip.interface_configuration,",
            "DecodeTcpIpInterfaceConfigurationWrapper,`r`n                  &g_tcpip.interface_configuration,")
        $tcpipSrc = $tcpipSrc.Replace(
            "DecodeCipTcpIpInterfaceHostName,`r`n                  &g_tcpip.hostname,",
            "DecodeTcpIpInterfaceHostNameWrapper,`r`n                  &g_tcpip.hostname,")
        [System.IO.File]::WriteAllText($tcpip, $tcpipSrc)
    }
}

function Ensure-Micro800RunIdleCompat {
    param(
        [string]$Root
    )

    $cipIo = Join-Path $Root "src\cip\cipioconnection.c"
    $apiH = Join-Path $Root "src\opener_api.h"
    $cmakeLists = Join-Path $Root "CMakeLists.txt"

    if (-not (Test-Path $cipIo)) {
        throw "Missing $cipIo"
    }

    $cipSrc = Get-Content $cipIo -Raw
    if ($cipSrc -notmatch 'OpenerConfigureMicro800RunIdleCompat') {
        $cipSrc = $cipSrc -replace '(bool CipRunIdleHeaderGetT2O\(void\) \{\s*\r?\n\s*return s_produce_run_idle;\s*\r?\n\})',
@'
$1

void OpenerConfigureMicro800RunIdleCompat(void) {
#if defined(OPENER_MICRO800_RUN_IDLE_COMPAT) && OPENER_MICRO800_RUN_IDLE_COMPAT
  CipRunIdleHeaderSetO2T(false);
  CipRunIdleHeaderSetT2O(false);
#endif
}
'@

    }

    if ($cipSrc -notmatch 'Applying implicit run/idle header compensation') {
        $cipSrc = $cipSrc -replace '(?s)(      diff_size \+= 2;\s*\r?\n    \}\s*\r?\n)\s*\r?\n    if\( s_consume_run_idle && \(data_size > 0\) && \(!is_heartbeat\) \) \{',
@'
$1
#if defined(OPENER_MICRO800_RUN_IDLE_COMPAT) && OPENER_MICRO800_RUN_IDLE_COMPAT
    {
      EipInt16 length_gap =
        (EipInt16)(data_size - (int)( (CipByteArray *) attribute->data )->length);
      if( (length_gap == 4) && !s_consume_run_idle ) {
        OPENER_TRACE_INFO(
          "Applying implicit run/idle header compensation for consuming data\n");
        data_size -= 4;
        diff_size += 4;
      }
    }
#endif

    if( s_consume_run_idle && (data_size > 0) && (!is_heartbeat) ) {
'@
    }

    if ($cipSrc -notmatch 'Micro800 forced Run/Idle header') {
        $recvAnchor = @'
    if(no_new_data) {
      return kEipStatusOk;
    }

    if(NotifyAssemblyConnectedDataReceived(connection_object->consuming_instance,
'@
        $recvInsert = @'
    if(no_new_data) {
      return kEipStatusOk;
    }

#if defined(OPENER_MICRO800_RUN_IDLE_COMPAT) && OPENER_MICRO800_RUN_IDLE_COMPAT
    {
      CipByteArray *consuming_instance_attributes =
        (CipByteArray *) connection_object->consuming_instance->attributes->data;

      if( (0 == s_consume_run_idle) &&
          (data_length == (consuming_instance_attributes->length + 4U)) ) {
        const EipUint8 *run_idle_data = data;
        EipUint32 nRunIdleBuf = GetUdintFromMessage(&run_idle_data);
        OPENER_TRACE_INFO(
          "Micro800 forced Run/Idle header: 0x%x\n",
          nRunIdleBuf);
        RunIdleChanged(nRunIdleBuf);
        g_run_idle_state = nRunIdleBuf;
        OPENER_TRACE_INFO(
          "Discarding forced 4-byte run/idle header from originator\n");
        data += 4;
        data_length = (EipUint16)(data_length - 4U);
      }
    }
#endif

    if(NotifyAssemblyConnectedDataReceived(connection_object->consuming_instance,
'@
        if ($cipSrc -notmatch [regex]::Escape($recvAnchor.Trim())) {
            throw 'cipioconnection.c receive-path anchor not found for Micro800 run-idle patch'
        }
        $cipSrc = $cipSrc.Replace($recvAnchor, $recvInsert)
    }

    Set-Content -Path $cipIo -Value $cipSrc -NoNewline

    if (Test-Path $apiH) {
        $apiSrc = Get-Content $apiH -Raw
        if ($apiSrc -notmatch 'OpenerConfigureMicro800RunIdleCompat') {
            $apiSrc = $apiSrc -replace '(bool CipRunIdleHeaderGetT2O\(void\);\s*\r?\n)',
@'
$1
/** @ingroup CIP_API
 * @brief Apply Rockwell Micro800/Logix run-idle defaults (O->T and T->O headers off).
 *
 * When OPENER_MICRO800_RUN_IDLE_COMPAT is enabled, also activates Forward Open
 * size compensation and receive-path stripping for undeclared 4-byte headers.
 */
void OpenerConfigureMicro800RunIdleCompat(void);

'@
            Set-Content -Path $apiH -Value $apiSrc -NoNewline
        }
    }

    if (Test-Path $cmakeLists) {
        $cmake = Get-Content $cmakeLists -Raw
        if ($cmake -notmatch 'OPENER_MICRO800_RUN_IDLE_COMPAT') {
            $cmake = $cmake -replace '(option\(OPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER[^\)]+\))',
                "`$1`r`noption(OPENER_MICRO800_RUN_IDLE_COMPAT `"Rockwell Micro800/Logix run-idle header workarounds`" ON)"
            $cmake = $cmake -replace '(if\(OPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER\)\s*\r?\n\s*add_definitions\(-DOPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER\)\s*\r?\n\s*endif\(\))',
@'
if(OPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER)
  add_definitions(-DOPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER)
endif()

if(OPENER_MICRO800_RUN_IDLE_COMPAT)
  add_definitions(-DOPENER_MICRO800_RUN_IDLE_COMPAT=1)
endif()
'@
            Set-Content -Path $cmakeLists -Value $cmake -NoNewline
        }
    }
}

function Ensure-SockaddrInfoNetworkOrder {
    param(
        [string]$Root
    )

    $cpf = Join-Path $Root "src\enet_encap\cpf.c"
    if (-not (Test-Path $cpf)) {
        throw "Missing $cpf"
    }

    $cpfSrc = Get-Content $cpf -Raw
    if ($cpfSrc -match 'GetSockaddrNetworkUintFromMessage') {
        return
    }

    $helper = @'

static CipUint GetSockaddrNetworkUintFromMessage(const CipOctet **const buffer_address) {
  const CipOctet *buffer = *buffer_address;
  CipUint data = (CipUint)(((CipUint)buffer[0] << 8) | (CipUint)buffer[1]);
  *buffer_address += 2;
  return data;
}

static CipUdint GetSockaddrNetworkUdintFromMessage(const CipOctet **const buffer_address) {
  const CipOctet *buffer = *buffer_address;
  CipUdint data = ((CipUdint)buffer[0] << 24) |
                  ((CipUdint)buffer[1] << 16) |
                  ((CipUdint)buffer[2] << 8) |
                  (CipUdint)buffer[3];
  *buffer_address += 4;
  return data;
}
'@

    $cpfSrc = $cpfSrc -replace '(\r?\nEipStatus CreateCommonPacketFormatStructure\()',
        "$helper`r`n`$1"
    $cpfSrc = $cpfSrc.Replace(
        "common_packet_format_data->address_info_item[j].sin_family =`r`n          (CipInt)GetUintFromMessage(&data);",
        "common_packet_format_data->address_info_item[j].sin_family =`r`n          (CipInt)GetSockaddrNetworkUintFromMessage(&data);")
    $cpfSrc = $cpfSrc.Replace(
        "common_packet_format_data->address_info_item[j].sin_port =`r`n          GetUintFromMessage(&data);",
        "common_packet_format_data->address_info_item[j].sin_port =`r`n          GetSockaddrNetworkUintFromMessage(&data);")
    $cpfSrc = $cpfSrc.Replace(
        "common_packet_format_data->address_info_item[j].sin_addr =`r`n          GetUdintFromMessage(&data);",
        "common_packet_format_data->address_info_item[j].sin_addr =`r`n          GetSockaddrNetworkUdintFromMessage(&data);")

    Set-Content -Path $cpf -Value $cpfSrc -NoNewline
}

Ensure-Micro800RunIdleCompat -Root $StageDir
Ensure-SockaddrInfoNetworkOrder -Root $StageDir

Write-Host "Patched OpENer staged source for NETBURNER."
