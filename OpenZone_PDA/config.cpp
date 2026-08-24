// OpenZone PDA -- a S.T.A.L.K.E.R.-style handheld device.
//
// Depends HARD on OpenZone_Core. That dependency is fatal by design: a mod
// declaring it and loading without the core gets a blocking "Addon X requires
// addon Y" dialog before the game starts, not a silently skipped pbo.
//
// The PDA declares three slots and owns none of what goes in them. Batteries
// are vanilla; antennas come from OpenZone Radio; data carriers can come from
// anywhere. What each attachment DOES is a table of classnames in JSON, so an
// admin can point the mod at items from mods we have never heard of.

class CfgSlots
{
    // The class is Slot_<name>; the bare `name` is what attachments[] and
    // inventorySlot[] match on.
    class Slot_OZ_Antenna
    {
        name = "OZ_Antenna";
        displayName = "$STR_OZ_SLOT_ANTENNA";
        ghostIcon = "set:dayz_inventory image:cable";
    };

    class Slot_OZ_DataCarrier
    {
        name = "OZ_DataCarrier";
        displayName = "$STR_OZ_SLOT_CARRIER";
        ghostIcon = "set:dayz_inventory image:memorycard";
    };
};

class CfgPatches
{
    class OpenZone_PDA
    {
        units[] = {"OZ_PDA_Novice", "OZ_DataCarrier_Base"};
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
            // The vanilla GPS model borrowed until dayz-3d delivers our own.
            "DZ_Gear_Navigation"
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
            // imageSets appear in Task 9 together with the UI they serve: an
            // imageset pointing at a texture that does not exist yet is a
            // broken reference the engine complains about on every boot.

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

        // Battery, antenna, data carrier. The battery slot is all the engine
        // needs to know: with plugType=1 and attachmentAction=1 it plugs the
        // device into whatever battery is attached and unplugs it on detach,
        // with no script involved.
        //
        // The other two carry no engine meaning at all -- they are ours, and
        // what fits in them is decided by JSON, not by inheritance.
        attachments[] = {"BatteryD", "OZ_Antenna", "OZ_DataCarrier"};

        class EnergyManager
        {
            hasIcon = 1;
            autoSwitchOffWhenInCargo = 1;
            // 0.5 per minute. A device profile can raise the effective drain;
            // this is the default the engine ticks with.
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

    // A data carrier the PDA can read. Ships as a base plus one concrete item
    // so the slot is testable out of the box; content mods add their own and
    // name them in the JSON table.
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
};
