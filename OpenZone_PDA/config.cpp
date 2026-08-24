// OpenZone PDA -- a S.T.A.L.K.E.R.-style handheld device.
//
// Depends HARD on OpenZone_Core. That dependency is fatal by design: a mod
// declaring it and loading without the core gets a blocking "Addon X requires
// addon Y" dialog before the game starts, not a silently skipped pbo.
//
// Three kinds of socket, not one list:
//
//   POWER    a battery. Vanilla slot; the engine plugs the device into it.
//   STORAGE  a data carrier. Its own slot: that is content, not capability.
//   MODULES  antenna, radiometer, dosimeter, and whatever other mods bring.
//            They share a LIMITED number of bays.
//
// The bay count is the tier lever. A rookie PDA has one and its owner chooses
// between long-range comms and a Geiger counter; a Duty PDA has three and
// carries all of it. A device with a slot per device type would offer no
// choice at all.
//
// What fits a bay, and what it does, is a table of classnames in JSON -- so a
// module can come from any mod, and an admin can point the PDA at an item we
// have never heard of.

class CfgSlots
{
    // The class is Slot_<name>; the bare `name` is what attachments[] and
    // inventorySlot[] match on.
    class Slot_OZ_DataCarrier
    {
        name = "OZ_DataCarrier";
        displayName = "$STR_OZ_SLOT_CARRIER";
        ghostIcon = "set:dayz_inventory image:memorycard";
    };

    class Slot_OZ_Module1
    {
        name = "OZ_Module1";
        displayName = "$STR_OZ_SLOT_MODULE";
        ghostIcon = "set:dayz_inventory image:cable";
    };

    class Slot_OZ_Module2
    {
        name = "OZ_Module2";
        displayName = "$STR_OZ_SLOT_MODULE";
        ghostIcon = "set:dayz_inventory image:cable";
    };

    class Slot_OZ_Module3
    {
        name = "OZ_Module3";
        displayName = "$STR_OZ_SLOT_MODULE";
        ghostIcon = "set:dayz_inventory image:cable";
    };
};

class CfgPatches
{
    class OpenZone_PDA
    {
        units[] =
        {
            "OZ_PDA_Novice",
            "OZ_PDA_Advanced",
            "OZ_DataCarrier_Chip",
            "OZ_Module_Radiometer",
            "OZ_Module_Dosimeter"
        };
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "JM_CF_Scripts",
            // The core's CfgPatches class name -- that string IS the addon
            // identity, and nothing else resolves to it.
            "OpenZone_Core",
            // Vanilla models borrowed until dayz-3d delivers our own.
            "DZ_Gear_Navigation",
            "DZ_Gear_Tools"
        };
    };
};

class CfgMods
{
    class OpenZone_PDA
    {
        dir = "OpenZone_PDA";
        name = "OpenZone PDA";
        author = "Zone Protocol";
        version = "0.1.0";
        type = "mod";
        storageVersion = 1;
        dependencies[] = {"Game", "World", "Mission"};
        defines[] = {"OPENZONE_PDA"};

        // Read by the ENGINE, not by script. Path is PBO-prefix-relative with
        // no leading slash; lookup is case-insensitive.
        inputs = "OpenZone_PDA/data/inputs.xml";

        class defs
        {
            // imageSets arrive with the UI they serve: an imageset pointing at
            // a texture that does not exist yet is a broken reference the
            // engine complains about on every boot.

            class gameScriptModule    { value = ""; files[] = {"OpenZone_PDA/scripts/3_Game"}; };
            class worldScriptModule   { value = ""; files[] = {"OpenZone_PDA/scripts/4_World"}; };
            class missionScriptModule { value = ""; files[] = {"OpenZone_PDA/scripts/5_Mission"}; };
        };
    };
};

class CfgVehicles
{
    class Inventory_Base;

    class OZ_PDA_Base : Inventory_Base
    {
        scope = 0;

        // Placeholder model. The real one comes from dayz-3d as a .p3d plus a
        // front-panel render used as the UI bezel -- both out of one .blend so
        // they cannot drift apart.
        model = "\dz\gear\navigation\GPSReceiver.p3d";

        itemSize[] = {2, 3};
        weight = 300;
        rotationFlags = 1;
        absorbency = 0.3;

        // Every bay is declared here; the profile decides how many of them a
        // given model actually exposes, and the script hides the rest. Slots
        // cannot be added at runtime, so the maximum lives in config and the
        // limit lives in JSON.
        attachments[] =
        {
            "BatteryD",
            "OZ_DataCarrier",
            "OZ_Module1",
            "OZ_Module2",
            "OZ_Module3"
        };

        class EnergyManager
        {
            hasIcon = 1;
            autoSwitchOffWhenInCargo = 1;
            // 0.5 per minute. Modules raise the effective drain through their
            // PowerFactor; this is the bare device.
            energyUsagePerSecond = 0.0083;
            // Stores nothing itself: it lives off the attached battery.
            energyStorageMax = 0;
            plugType = 1;             // PLUG_BATTERY_SLOT
            attachmentAction = 1;     // PLUG_THIS_INTO_ATTACHMENT
            updateInterval = 30;
        };
    };

    class OZ_PDA_Novice : OZ_PDA_Base
    {
        scope = 2;
        displayName = "$STR_OZ_PDA_NOVICE";
        descriptionShort = "$STR_OZ_PDA_NOVICE_DESC";
    };

    class OZ_PDA_Advanced : OZ_PDA_Base
    {
        scope = 2;
        displayName = "$STR_OZ_PDA_ADVANCED";
        descriptionShort = "$STR_OZ_PDA_ADVANCED_DESC";
    };

    // ------------------------------------------------------------- storage

    class OZ_DataCarrier_Base : Inventory_Base
    {
        scope = 0;
        model = "\dz\gear\navigation\Map.p3d";
        itemSize[] = {1, 1};
        weight = 20;
        rotationFlags = 1;
        inventorySlot[] = {"OZ_DataCarrier"};
    };

    class OZ_DataCarrier_Chip : OZ_DataCarrier_Base
    {
        scope = 2;
        displayName = "$STR_OZ_CARRIER_CHIP";
        descriptionShort = "$STR_OZ_CARRIER_CHIP_DESC";
    };

    // ------------------------------------------------------------- modules
    //
    // A module fits ANY bay: one inventorySlot[] listing all three, so the
    // player is not made to remember which hole a given chip belongs in.

    class OZ_Module_Base : Inventory_Base
    {
        scope = 0;
        model = "\dz\gear\tools\Battery9V.p3d";
        itemSize[] = {1, 1};
        weight = 60;
        rotationFlags = 1;
        inventorySlot[] = {"OZ_Module1", "OZ_Module2", "OZ_Module3"};
    };

    // Geiger counter: measures the field around you, right now.
    class OZ_Module_Radiometer : OZ_Module_Base
    {
        scope = 2;
        displayName = "$STR_OZ_MOD_RADIOMETER";
        descriptionShort = "$STR_OZ_MOD_RADIOMETER_DESC";
    };

    // Injected sensor: measures the dose already in your body.
    class OZ_Module_Dosimeter : OZ_Module_Base
    {
        scope = 2;
        displayName = "$STR_OZ_MOD_DOSIMETER";
        descriptionShort = "$STR_OZ_MOD_DOSIMETER_DESC";
    };

    // Short-range antenna. Both halves of the transponder need one: it is
    // what lets the device be seen and what lets it see. Longer-range ones
    // come from OpenZone Radio and simply declare a larger RangeM.
    class OZ_Module_Antenna : OZ_Module_Base
    {
        scope = 2;
        displayName = "$STR_OZ_MOD_ANTENNA";
        descriptionShort = "$STR_OZ_MOD_ANTENNA_DESC";
    };
};
