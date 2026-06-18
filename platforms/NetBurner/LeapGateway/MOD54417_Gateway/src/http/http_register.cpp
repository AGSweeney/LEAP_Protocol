/*

 * LEAP Gateway (NetBurner MOD5441X)

 *

 * SPDX-License-Identifier: MIT

 */



#ifdef LEAPGATEWAY_MAIN_TU



CallBackFunctionPageHandler gNetworkConfigSaveHandler("api/network/config/save", HandleNetworkConfigSaveApi, MatchNetworkConfigSaveApi, tGet, 0, true);

CallBackFunctionPageHandler gNetworkConfigHandler("api/network/config", HandleNetworkConfigApi, MatchNetworkConfigApi, tGet, 0, true);

CallBackFunctionPageHandler gMappingConfigSaveHandler("api/mapping/config/save", HandleMappingConfigSaveApi, MatchMappingConfigSaveApi, tGet, 0, true);

CallBackFunctionPageHandler gMappingConfigHandler("api/mapping/config", HandleMappingConfigApi, MatchMappingConfigApi, tGet, 0, true);



CallBackFunctionPageHandler gLeapStatusHandler("api/leap/status", HandleLeapStatusApi, MatchLeapStatusApi, tGet, 0, true);

CallBackFunctionPageHandler gLeapStatusV1Handler("api/v1/status", HandleLeapStatusApi, MatchLeapStatusV1Api, tGet, 0, true);

CallBackFunctionPageHandler gLeapConnectHandler("api/leap/connect", HandleLeapConnectApi, MatchLeapConnectApi, tGet, 0, true);

CallBackFunctionPageHandler gLeapConnectV1Handler("api/v1/leap/connect", HandleLeapConnectApi, MatchLeapConnectV1Api, tGet, 0, true);

CallBackFunctionPageHandler gLeapDisconnectHandler("api/leap/disconnect", HandleLeapDisconnectApi, MatchLeapDisconnectApi, tGet, 0, true);

CallBackFunctionPageHandler gLeapDisconnectV1Handler("api/v1/leap/disconnect", HandleLeapDisconnectApi, MatchLeapDisconnectV1Api, tGet, 0, true);

CallBackFunctionPageHandler gLeapDiscoverHandler("api/leap/discover", HandleLeapDiscoverApi, MatchLeapDiscoverApi, tGet, 0, true);

CallBackFunctionPageHandler gLeapDiscoverV1Handler("api/v1/leap/discover", HandleLeapDiscoverApi, MatchLeapDiscoverV1Api, tGet, 0, true);

CallBackFunctionPageHandler gLeapPeersHandler("api/leap/peers", HandleLeapPeersApi, MatchLeapPeersApi, tGet, 0, true);

CallBackFunctionPageHandler gLeapPeersV1Handler("api/v1/leap/peers", HandleLeapPeersApi, MatchLeapPeersV1Api, tGet, 0, true);

CallBackFunctionPageHandler gLeapIoHandler("api/leap/io", HandleLeapIoApi, MatchLeapIoApi, tGet, 0, true);

CallBackFunctionPageHandler gLeapIoV1Handler("api/v1/io", HandleLeapIoApi, MatchLeapIoV1Api, tGet, 0, true);

CallBackFunctionPageHandler gConfigPersistHandler("api/config/persist", HandleConfigPersistApi, MatchConfigPersistApi, tGet, 0, true);

CallBackFunctionPageHandler gConfigPersistV1Handler("api/v1/config/apply", HandleConfigPersistApi, MatchConfigPersistV1Api, tGet, 0, true);

CallBackFunctionPageHandler gSystemRebootHandler("api/system/reboot", HandleSystemRebootApi, MatchSystemRebootApi, tGet, 0, true);

CallBackFunctionPageHandler gSystemRebootV1Handler("api/v1/system/reboot", HandleSystemRebootApi, MatchSystemRebootV1Api, tGet, 0, true);



#endif // LEAPGATEWAY_MAIN_TU

