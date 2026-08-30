// PDA tab for the OpenZone VPP admin window -- an OPTIONAL pbo of its own.
// Hard-depends on the core VPP tab (it extends OZ_VppAdminMenu) and on the
// PDA (it edits PDA configs through classes the PDA declares). Servers
// without VPP simply do not load either tab pbo; the mods themselves never
// require VPP.

class CfgPatches
{
    class OpenZone_PDA_VPP
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "DZ_Scripts",
            "OpenZone_Core",
            "OpenZone_PDA",
            "OpenZone_VPP",
            "DZM_VPPAdminToolsScripts"
        };
    };
};

class CfgMods
{
    class OpenZone_PDA_VPP
    {
        dir = "OpenZone_PDA_VPP";
        name = "OpenZone PDA VPP Tab";
        author = "Zone Protocol";
        version = "0.1.0";
        type = "mod";

        dependencies[] = {"Mission"};

        class defs
        {
            class missionScriptModule { value = ""; files[] = {"OpenZone_PDA_VPP/scripts/5_Mission"}; };
        };
    };
};
