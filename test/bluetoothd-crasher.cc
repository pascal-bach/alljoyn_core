/**
 * @file
 * Sample implementation of an AllJoyn client.
 */

/******************************************************************************
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/
#include <qcc/platform.h>

#include <assert.h>
#include <list>
#include <set>
#include <stdio.h>

#include <pthread.h>

#include <qcc/Environ.h>
#include <qcc/Event.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Thread.h>
#include <qcc/time.h>
#include <qcc/Util.h>

#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/DBusStd.h>

#include "../daemon/BDAddress.h"

#include <Status.h>

#define QCC_MODULE "ALLJOYN"

#define METHODCALL_TIMEOUT 30000

using namespace std;
using namespace qcc;
using namespace ajn;

namespace test {
static struct {
    struct {
        struct {
            const InterfaceDescription* interface;
            // Methods (not all; only those needed)
            const InterfaceDescription::Member* DefaultAdapter;
            const InterfaceDescription::Member* ListAdapters;
            // Signals
            const InterfaceDescription::Member* AdapterAdded;
            const InterfaceDescription::Member* AdapterRemoved;
            const InterfaceDescription::Member* DefaultAdapterChanged;
        } Manager;

        struct {
            const InterfaceDescription* interface;
            // Methods (not all; only those needed)
            const InterfaceDescription::Member* AddRecord;
            const InterfaceDescription::Member* RemoveRecord;
        } Service;

        struct {
            const InterfaceDescription* interface;
            // Methods (not all; only those needed)
            const InterfaceDescription::Member* CreateDevice;
            const InterfaceDescription::Member* FindDevice;
            const InterfaceDescription::Member* GetProperties;
            const InterfaceDescription::Member* ListDevices;
            const InterfaceDescription::Member* RemoveDevice;
            const InterfaceDescription::Member* SetProperty;
            const InterfaceDescription::Member* StartDiscovery;
            const InterfaceDescription::Member* StopDiscovery;
            // Signals
            const InterfaceDescription::Member* DeviceCreated;
            const InterfaceDescription::Member* DeviceDisappeared;
            const InterfaceDescription::Member* DeviceFound;
            const InterfaceDescription::Member* DeviceRemoved;
            const InterfaceDescription::Member* PropertyChanged;
        } Adapter;

        struct {
            const InterfaceDescription* interface;
            // Methods (not all; only those needed)
            const InterfaceDescription::Member* DiscoverServices;
            const InterfaceDescription::Member* GetProperties;
            // Signals
            const InterfaceDescription::Member* DisconnectRequested;
            const InterfaceDescription::Member* PropertyChanged;
        } Device;
    } bluez;
} org;
}

struct InterfaceDesc {
    AllJoynMessageType type;
    const char* name;
    const char* inputSig;
    const char* outSig;
    const char* argNames;
    uint8_t annotation;
};

struct InterfaceTable {
    const char* ifcName;
    const InterfaceDesc* desc;
    size_t tableSize;
};


const char* bzBusName = "org.bluez";
const char* bzMgrObjPath = "/";
const char* bzManagerIfc = "org.bluez.Manager";
const char* bzServiceIfc = "org.bluez.Service";
const char* bzAdapterIfc = "org.bluez.Adapter";
const char* bzDeviceIfc = "org.bluez.Device";

const InterfaceDesc bzManagerIfcTbl[] = {
    { MESSAGE_METHOD_CALL, "DefaultAdapter",        NULL, "o",     NULL, 0 },
    { MESSAGE_METHOD_CALL, "FindAdapter",           "s",  "o",     NULL, 0 },
    { MESSAGE_METHOD_CALL, "GetProperties",         NULL, "a{sv}", NULL, 0 },
    { MESSAGE_METHOD_CALL, "ListAdapters",          NULL, "ao",    NULL, 0 },
    { MESSAGE_SIGNAL,      "AdapterAdded",          "o",  NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "AdapterRemoved",        "o",  NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "DefaultAdapterChanged", "o",  NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "PropertyChanged",       "sv", NULL,    NULL, 0 }
};

const InterfaceDesc bzAdapterIfcTbl[] = {
    { MESSAGE_METHOD_CALL, "CancelDeviceCreation", "s",      NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "CreateDevice",         "s",      "o",     NULL, 0 },
    { MESSAGE_METHOD_CALL, "CreatePairedDevice",   "sos",    "o",     NULL, 0 },
    { MESSAGE_METHOD_CALL, "FindDevice",           "s",      "o",     NULL, 0 },
    { MESSAGE_METHOD_CALL, "GetProperties",        NULL,     "a{sv}", NULL, 0 },
    { MESSAGE_METHOD_CALL, "ListDevices",          NULL,     "ao",    NULL, 0 },
    { MESSAGE_METHOD_CALL, "RegisterAgent",        "os",     NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "ReleaseSession",       NULL,     NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "RemoveDevice",         "o",      NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "RequestSession",       NULL,     NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "SetProperty",          "sv",     NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "StartDiscovery",       NULL,     NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "StopDiscovery",        NULL,     NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "UnregisterAgent",      "o",      NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "DeviceCreated",        "o",      NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "DeviceDisappeared",    "s",      NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "DeviceFound",          "sa{sv}", NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "DeviceRemoved",        "o",      NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "PropertyChanged",      "sv",     NULL,    NULL, 0 }
};

const InterfaceDesc bzServiceIfcTbl[] = {
    { MESSAGE_METHOD_CALL, "AddRecord",            "s",  "u",  NULL, 0 },
    { MESSAGE_METHOD_CALL, "CancelAuthorization",  NULL, NULL, NULL, 0 },
    { MESSAGE_METHOD_CALL, "RemoveRecord",         "u",  NULL, NULL, 0 },
    { MESSAGE_METHOD_CALL, "RequestAuthorization", "su", NULL, NULL, 0 },
    { MESSAGE_METHOD_CALL, "UpdateRecord",         "us", NULL, NULL, 0 }
};

const InterfaceDesc bzDeviceIfcTbl[] = {
    { MESSAGE_METHOD_CALL, "CancelDiscovery",     NULL, NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "Disconnect",          NULL, NULL,    NULL, 0 },
    { MESSAGE_METHOD_CALL, "DiscoverServices",    "s",  "a{us}", NULL, 0 },
    { MESSAGE_METHOD_CALL, "GetProperties",       NULL, "a{sv}", NULL, 0 },
    { MESSAGE_METHOD_CALL, "SetProperty",         "sv", NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "DisconnectRequested", NULL, NULL,    NULL, 0 },
    { MESSAGE_SIGNAL,      "PropertyChanged",     "sv", NULL,    NULL, 0 }
};

const InterfaceTable ifcTables[] = {
    { "org.bluez.Manager", bzManagerIfcTbl, ArraySize(bzManagerIfcTbl) },
    { "org.bluez.Adapter", bzAdapterIfcTbl, ArraySize(bzAdapterIfcTbl) },
    { "org.bluez.Service", bzServiceIfcTbl, ArraySize(bzServiceIfcTbl) },
    { "org.bluez.Device",  bzDeviceIfcTbl,  ArraySize(bzDeviceIfcTbl)  }
};

const size_t ifcTableSize = ArraySize(ifcTables);


class MyBusListener : public BusListener, public SessionListener {
  public:

    MyBusListener() { }

    void NameOwnerChanged(const char* name, const char* previousOwner, const char* newOwner)
    {
        if (previousOwner && !newOwner && (strcmp(name, bzBusName) == 0)) {
            printf("org.bluez has crashed.  Stopping...\n");
            exit(0);
        }
    }

  private:
};



class Crasher : public Thread, public MessageReceiver {
  public:
    Crasher(BusAttachment& bus, ProxyBusObject& bzAdapterObj, bool wait) :
        bus(bus),
        bzAdapterObj(bzAdapterObj),
        wait(wait),
        discovering(false)
    {
        QStatus status = bus.RegisterSignalHandler(this,
                                                   static_cast<MessageReceiver::SignalHandler>(&Crasher::DeviceFoundSignalHandler),
                                                   test::org.bluez.Adapter.DeviceFound, NULL);
        if (status != ER_OK) {
            printf("Failed to register signal handler: %s\n", QCC_StatusText(status));
            exit(1);
        }

        status = bus.RegisterSignalHandler(this,
                                           static_cast<MessageReceiver::SignalHandler>(&Crasher::PropertyChangedSignalHandler),
                                           test::org.bluez.Adapter.PropertyChanged, NULL);
        if (status != ER_OK) {
            printf("Failed to register signal handler: %s\n", QCC_StatusText(status));
            exit(1);
        }
        pthread_cond_init(&notDiscovering, NULL);
        pthread_mutex_init(&discMutex, NULL);
    }

    ~Crasher()
    {
        pthread_mutex_destroy(&discMutex);
        pthread_cond_destroy(&notDiscovering);
    }


    void DeviceFoundSignalHandler(const InterfaceDescription::Member* member,
                                  const char* sourcePath,
                                  Message& msg);
    void PropertyChangedSignalHandler(const InterfaceDescription::Member* member,
                                      const char* sourcePath,
                                      Message& msg);

    void* Run(void* arg);

  private:
    BusAttachment& bus;
    ProxyBusObject& bzAdapterObj;
    set<BDAddress> foundSet;
    list<BDAddress> checkList;
    Event newAddr;
    bool wait;
    bool discovering;
    pthread_cond_t notDiscovering;
    pthread_mutex_t discMutex;
};


void Crasher::DeviceFoundSignalHandler(const InterfaceDescription::Member* member,
                                       const char* sourcePath,
                                       Message& msg)
{
    BDAddress addr(msg->GetArg(0)->v_string.str);
    if (foundSet.find(addr) == foundSet.end()) {
        pthread_mutex_lock(&discMutex);
        foundSet.insert(addr);
        checkList.push_back(addr);
        pthread_mutex_unlock(&discMutex);
        printf("Found: %s\n", addr.ToString().c_str());
        if (foundSet.size() == 1) {
            newAddr.SetEvent();
        }
    }
}


void Crasher::PropertyChangedSignalHandler(const InterfaceDescription::Member* member,
                                           const char* sourcePath,
                                           Message& msg)
{
    const char* property;
    const MsgArg* value;

    msg->GetArgs("sv", &property, &value);

    if (strcmp(property, "Discovering") == 0) {
        value->Get("b", &discovering);
        printf("Discovering %s.\n", discovering ? "on" : "off");

        if (wait && !discovering) {
            pthread_cond_signal(&notDiscovering);
        }
    }
}


void* Crasher::Run(void* arg)
{
    QStatus status = ER_OK;

    status = Event::Wait(newAddr);
    if (status != ER_OK) {
        printf("Wait failed: %s\n", QCC_StatusText(status));
        bzAdapterObj.MethodCall(*test::org.bluez.Adapter.StopDiscovery, NULL, 0);
        return (void*) 1;
    }

    list<BDAddress>::iterator check;
    String objPath;
    ProxyBusObject deviceObject;
    MsgArg allSrv("s", "");

    while (!IsStopping()) {
        pthread_mutex_lock(&discMutex);
        if (wait && discovering) {
            printf("waiting for discovery to stop...\n");
            pthread_cond_wait(&notDiscovering, &discMutex);
        }
        for (check = checkList.begin(); (!wait || !discovering) && check != checkList.end(); ++check) {
            pthread_mutex_unlock(&discMutex);
            printf("Checking: %s\n", check->ToString().c_str());
            MsgArg arg("s", check->ToString().c_str());
            Message reply(bus);
            status = bzAdapterObj.MethodCall(*test::org.bluez.Adapter.FindDevice, &arg, 1, reply);
            if (status != ER_OK) {
                status = bzAdapterObj.MethodCall(*test::org.bluez.Adapter.CreateDevice, &arg, 1, reply);
            }
            if (status != ER_OK) {
                if (status == ER_BUS_REPLY_IS_ERROR_MESSAGE) {
                    String errMsg;
                    const char* errName = reply->GetErrorName(&errMsg);
                    printf("Failed find/create %s: %s - %s\n", check->ToString().c_str(), errName, errMsg.c_str());
                    if (strcmp(errName, "org.freedesktop.DBus.Error.NameHasNoOwner") == 0) {
                        printf("bluetoothd crashed\n");
                        exit(0);
                    }
                } else {
                    printf("Failed find/create %s: %s\n", check->ToString().c_str(), QCC_StatusText(status));
                }
                pthread_mutex_lock(&discMutex);
                continue;
            }
            objPath = reply->GetArg(0)->v_objPath.str;
            deviceObject = ProxyBusObject(bus, bzBusName, objPath.c_str(), 0);
            deviceObject.AddInterface(*test::org.bluez.Device.interface);

            status = deviceObject.MethodCall(*test::org.bluez.Device.DiscoverServices, &allSrv, 1, reply);
            if (status != ER_OK) {
                if (status == ER_BUS_REPLY_IS_ERROR_MESSAGE) {
                    String errMsg;
                    const char* errName = reply->GetErrorName(&errMsg);
                    printf("Failed to get service info: %s - %s\n", errName, errMsg.c_str());
                } else {
                    printf("Failed to get service info: %s\n", QCC_StatusText(status));
                }
            }

            arg.Set("o", objPath.c_str());
            bzAdapterObj.MethodCall(*test::org.bluez.Adapter.RemoveDevice, &arg, 1, reply);

            pthread_mutex_lock(&discMutex);
        }
        pthread_mutex_unlock(&discMutex);
        Sleep(500 + Rand32() % 500);
    }

    bzAdapterObj.MethodCall(*test::org.bluez.Adapter.StopDiscovery, NULL, 0);
    return (void*) 0;
}


int main(int argc, char** argv)
{
    QStatus status;
    Environ* env = Environ::GetAppEnviron();
    bool wait = false;

#ifdef ANDROID
    qcc::String connectArgs(env->Find("DBUS_SYSTEM_BUS_ADDRESS",
                                      "unix:path=/dev/socket/dbus"));
#else
    qcc::String connectArgs(env->Find("DBUS_SYSTEM_BUS_ADDRESS",
                                      "unix:path=/var/run/dbus/system_bus_socket"));
#endif

    if (argc == 2) {
        if (strcmp(argv[1], "-w") == 0) {
            wait = true;
        }
    }


    /* Create message bus */
    BusAttachment bus("bluetoothd-crasher");

    status = bus.Start();
    if (status != ER_OK) {
        printf("Failed to start bus: %s\n", QCC_StatusText(status));
        return 1;
    }

    status = bus.Connect(connectArgs.c_str());
    if (status != ER_OK) {
        printf("Failed to connect bus: %s\n", QCC_StatusText(status));
        return 1;
    }

    status = bus.AddMatch("type='signal',sender='org.bluez',interface='org.bluez.Adapter'");
    if (status != ER_OK) {
        printf("Failed to add match rule: %s\n", QCC_StatusText(status));
        return 1;
    }

    ProxyBusObject bzManagerObj(bus, bzBusName, bzMgrObjPath, 0);
    ProxyBusObject bzAdapterObj;

    Message reply(bus);
    vector<MsgArg> args;

    size_t tableIndex, member;

    for (tableIndex = 0; tableIndex < ifcTableSize; ++tableIndex) {
        InterfaceDescription* ifc;
        const InterfaceTable& table(ifcTables[tableIndex]);
        bus.CreateInterface(table.ifcName, ifc);

        if (ifc) {
            for (member = 0; member < table.tableSize; ++member) {
                ifc->AddMember(table.desc[member].type,
                               table.desc[member].name,
                               table.desc[member].inputSig,
                               table.desc[member].outSig,
                               table.desc[member].argNames,
                               table.desc[member].annotation);
            }
            ifc->Activate();

            if (table.desc == bzManagerIfcTbl) {
                test::org.bluez.Manager.interface =             ifc;
                test::org.bluez.Manager.DefaultAdapter =        ifc->GetMember("DefaultAdapter");
                test::org.bluez.Manager.ListAdapters =          ifc->GetMember("ListAdapters");
                test::org.bluez.Manager.AdapterAdded =          ifc->GetMember("AdapterAdded");
                test::org.bluez.Manager.AdapterRemoved =        ifc->GetMember("AdapterRemoved");
                test::org.bluez.Manager.DefaultAdapterChanged = ifc->GetMember("DefaultAdapterChanged");

            } else if (table.desc == bzAdapterIfcTbl) {
                test::org.bluez.Adapter.interface =          ifc;
                test::org.bluez.Adapter.CreateDevice =       ifc->GetMember("CreateDevice");
                test::org.bluez.Adapter.FindDevice =         ifc->GetMember("FindDevice");
                test::org.bluez.Adapter.GetProperties =      ifc->GetMember("GetProperties");
                test::org.bluez.Adapter.ListDevices =        ifc->GetMember("ListDevices");
                test::org.bluez.Adapter.RemoveDevice =       ifc->GetMember("RemoveDevice");
                test::org.bluez.Adapter.SetProperty =        ifc->GetMember("SetProperty");
                test::org.bluez.Adapter.StartDiscovery =     ifc->GetMember("StartDiscovery");
                test::org.bluez.Adapter.StopDiscovery =      ifc->GetMember("StopDiscovery");
                test::org.bluez.Adapter.DeviceCreated =      ifc->GetMember("DeviceCreated");
                test::org.bluez.Adapter.DeviceDisappeared =  ifc->GetMember("DeviceDisappeared");
                test::org.bluez.Adapter.DeviceFound =        ifc->GetMember("DeviceFound");
                test::org.bluez.Adapter.DeviceRemoved =      ifc->GetMember("DeviceRemoved");
                test::org.bluez.Adapter.PropertyChanged =    ifc->GetMember("PropertyChanged");

            } else if (table.desc == bzServiceIfcTbl) {
                test::org.bluez.Service.interface =          ifc;
                test::org.bluez.Service.AddRecord =          ifc->GetMember("AddRecord");
                test::org.bluez.Service.RemoveRecord =       ifc->GetMember("RemoveRecord");

            } else {
                test::org.bluez.Device.interface =           ifc;
                test::org.bluez.Device.DiscoverServices =    ifc->GetMember("DiscoverServices");
                test::org.bluez.Device.GetProperties =       ifc->GetMember("GetProperties");
                test::org.bluez.Device.DisconnectRequested = ifc->GetMember("DisconnectRequested");
                test::org.bluez.Device.PropertyChanged =     ifc->GetMember("PropertyChanged");
            }
        }
    }

    bzManagerObj.AddInterface(*test::org.bluez.Manager.interface);


    status = bzManagerObj.MethodCall(*test::org.bluez.Manager.DefaultAdapter, NULL, 0, reply);
    if (status != ER_OK) {
        printf("bzManagerObj.MethodCall() failed: %s\n", QCC_StatusText(status));
        return 1;
    }

    String adapterObjPath = reply->GetArg(0)->v_objPath.str;

    bzAdapterObj = ProxyBusObject(bus, bzBusName, adapterObjPath.c_str(), 0);
    bzAdapterObj.AddInterface(*test::org.bluez.Adapter.interface);

    Crasher crasher(bus, bzAdapterObj, wait);
    crasher.Start();


    status = bzAdapterObj.MethodCall(*test::org.bluez.Adapter.StartDiscovery, NULL, 0);
    if (status != ER_OK) {
        printf("Failed to start discovery: %s\n", QCC_StatusText(status));
        return 1;
    }

    crasher.Join();

    bzAdapterObj.MethodCall(*test::org.bluez.Adapter.StopDiscovery, NULL, 0);

    return 0;
}
