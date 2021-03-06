#include "scriptMenu.h"

#include <filesystem>
#include <fmt/core.h>
#include <inc/natives.h>
#include <menu.h>

#include "Constants.h"
#include "Memory/VehicleExtensions.hpp"
#include "Memory/Offsets.hpp"
#include "Util/Logger.hpp"
#include "Util/UIUtils.h"
#include "Util/MathExt.h"

#include "script.h"
#include "scriptSettings.h"
#include "gearInfo.h"
#include "Util/ScriptUtils.h"
#include "Util/Strings.h"


extern NativeMenu::Menu menu;
extern ScriptSettings settings;

extern Vehicle currentVehicle;
extern VehicleExtensions ext;

extern std::string gearConfigDir;

extern std::vector<GearInfo> gearConfigs;
extern std::vector<std::pair<Vehicle, GearInfo>> currentConfigs;

template <typename T>
void incVal(T& val, const T max, const T step) {
    if (val + step > max) return;
    val += step;
}

template <typename T>
void decVal(T& val, const T min, const T step) {
    if (val - step < min) return;
    val -= step;
}

bool GetKbEntryFloat(float& val) {
    UI::Notify(INFO, "Enter value");
    MISC::DISPLAY_ONSCREEN_KEYBOARD(LOCALIZATION::GET_CURRENT_LANGUAGE() == 0, "FMMC_KEY_TIP8", "",
        fmt::format("{:f}", val).c_str(), "", "", "", 64);
    while (MISC::UPDATE_ONSCREEN_KEYBOARD() == 0) {
        WAIT(0);
    }
    if (!MISC::GET_ONSCREEN_KEYBOARD_RESULT()) {
        UI::Notify(INFO, "Cancelled value entry");
        return false;
    }

    std::string floatStr = MISC::GET_ONSCREEN_KEYBOARD_RESULT();
    if (floatStr.empty()) {
        UI::Notify(INFO, "Cancelled value entry");
        return false;
    }

    char* pEnd;
    float parsedValue = strtof(floatStr.c_str(), &pEnd);

    if (parsedValue == 0.0f && *pEnd != 0) {
        UI::Notify(INFO, "Failed to parse entry.");
        return false;
    }

    val = parsedValue;
    return true;
}

std::string GetKbEntryString(const std::string& existingString) {
    std::string val;
    UI::Notify(INFO, "Enter value");
    MISC::DISPLAY_ONSCREEN_KEYBOARD(LOCALIZATION::GET_CURRENT_LANGUAGE() == 0, "FMMC_KEY_TIP8", "",
        existingString.c_str(), "", "", "", 64);
    while (MISC::UPDATE_ONSCREEN_KEYBOARD() == 0) {
        WAIT(0);
    }
    if (!MISC::GET_ONSCREEN_KEYBOARD_RESULT()) {
        UI::Notify(INFO, "Cancelled value entry");
        return {};
    }

    std::string enteredVal = MISC::GET_ONSCREEN_KEYBOARD_RESULT();
    if (enteredVal.empty()) {
        UI::Notify(INFO, "Cancelled value entry");
        return {};
    }

    return enteredVal;
}

void applyConfig(const GearInfo& config, Vehicle vehicle, bool notify, bool updateCurrent) {
    ext.SetTopGear(vehicle, config.TopGear);
    ext.SetDriveMaxFlatVel(vehicle, config.DriveMaxVel);
    ext.SetInitialDriveMaxFlatVel(vehicle, config.DriveMaxVel / 1.2f);
    ext.SetGearRatios(vehicle, config.Ratios);
    if (notify) {
        UI::Notify(INFO, fmt::format("[{}] applied to current {}",
            config.Description.c_str(), Util::GetFormattedVehicleModelName(vehicle).c_str()));
    }

    if (updateCurrent) {
        auto currCfgCombo = std::find_if(currentConfigs.begin(), currentConfigs.end(), [=](const auto& cfg) {return cfg.first == vehicle; });

        if (currCfgCombo != currentConfigs.end()) {
            auto& currentConfig = currCfgCombo->second;
            currentConfig.TopGear = config.TopGear;
            currentConfig.DriveMaxVel = config.DriveMaxVel;
            currentConfig.Ratios = config.Ratios;
        }
        else {
            logger.Write(DEBUG, "[Management] 0x%X not found?", vehicle);
        }
    }
}

std::vector<std::string> printInfo(const GearInfo& info) {
    uint8_t topGear = info.TopGear;
    auto ratios = info.Ratios;
    //float maxVel = (fInitialDriveMaxFlatVel * 1.2f) / 0.9f;
    float maxVel = info.DriveMaxVel;

    std::string loadType;
    switch (info.LoadType) {
        case LoadType::Plate: loadType = "Plate"; break;
        case LoadType::Model: loadType = "Model"; break;
        case LoadType::None: loadType = "None"; break;
    }

    std::vector<std::string> lines = {
        info.Description,
        fmt::format("For: {}", info.ModelName.c_str()),
        fmt::format("Plate: {}", info.LoadType == LoadType::Plate ? info.LicensePlate.c_str() : "Any"),
        fmt::format("Load type: {}", loadType.c_str()),
        fmt::format("Top gear: {}", topGear),
        "",
        "Gear ratios:",
    };

    for (uint8_t i = 0; i <= topGear; ++i) {
        std::string prefix;
        if (i == 0) {
            prefix = "Reverse";
        }
        else if (i == 1) {
            prefix = "1st";
        }
        else if (i == 2) {
            prefix = "2nd";
        }
        else if (i == 3) {
            prefix = "3rd";
        }
        else {
            prefix = fmt::format("{}th", i);
        }
        lines.push_back(fmt::format("{}: {:.2f} (rev limit: {:.0f} kph)",
            prefix.c_str(), ratios[i], 3.6f * maxVel / ratios[i]));
    }

    return lines;
}

std::vector<std::string> printGearStatus(Vehicle vehicle, uint8_t tunedGear) {
    uint8_t topGear = ext.GetTopGear(vehicle);
    uint16_t currentGear = ext.GetGearCurr(vehicle);
    float maxVel = ext.GetDriveMaxFlatVel(vehicle);
    auto ratios = ext.GetGearRatios(vehicle);

    std::vector<std::string> lines = {
        fmt::format("Top gear: {}", topGear),
        fmt::format("Final drive: {:.1f} kph", maxVel * 3.6f),
        fmt::format("Current gear: {}", currentGear),
        "",
        "Gear ratios:",
    };

    for (uint8_t i = 0; i <= topGear; ++i) {
        std::string prefix;
        if (i == 0) {
            prefix = "Reverse";
        }
        else if (i == 1) {
            prefix = "1st";
        }
        else if (i == 2) {
            prefix = "2nd";
        }
        else if (i == 3) {
            prefix = "3rd";
        }
        else {
            prefix = fmt::format("{}th", i);
        }
        lines.push_back(fmt::format("{}{}: {:.2f} (rev limit: {:.0f} kph)", i == tunedGear ? "~b~" : "",
            prefix.c_str(), ratios[i], 3.6f * maxVel / ratios[i]));
    }

    return lines;
}

void promptSave(Vehicle vehicle, LoadType loadType) {
    uint8_t topGear = ext.GetTopGear(vehicle);
    float driveMaxVel = ext.GetDriveMaxFlatVel(vehicle);
    std::vector<float> ratios = ext.GetGearRatios(vehicle);

    std::string modelName = VEHICLE::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL(ENTITY::GET_ENTITY_MODEL(vehicle));

    std::string saveFileProto = fmt::format("{}_{}_{:.0f}kph", modelName.c_str(), topGear,
        3.6f * driveMaxVel / ratios[topGear]);
    std::string saveFileBase;
    std::string saveFile;

    std::string licensePlate;
    switch (loadType) {
        case LoadType::Plate:   licensePlate = VEHICLE::GET_VEHICLE_NUMBER_PLATE_TEXT(vehicle); break;
        case LoadType::Model:   licensePlate = LoadName::Model;  break;
        case LoadType::None:    licensePlate = LoadName::None; break;
    }

    UI::Notify(INFO, "Enter description");
    WAIT(0);
    MISC::DISPLAY_ONSCREEN_KEYBOARD(LOCALIZATION::GET_CURRENT_LANGUAGE() == 0, "FMMC_KEY_TIP8", "", "", "", "", "", 64);
    while (MISC::UPDATE_ONSCREEN_KEYBOARD() == 0) WAIT(0);
    if (!MISC::GET_ONSCREEN_KEYBOARD_RESULT()) {
        UI::Notify(INFO, "Cancelled save");
        return;
    }

    std::string description = MISC::GET_ONSCREEN_KEYBOARD_RESULT();
    std::string illegalChars = "\\/:?\"<>|";
    if (description.empty()) {
        UI::Notify(INFO, "No description entered, using default");
        std::string carName = Util::GetFormattedVehicleModelName(vehicle);
        description = fmt::format("{} - {} gears - {:.0f} kph",
            carName.c_str(), topGear,
            3.6f * driveMaxVel / ratios[topGear]);
        saveFileBase = StrUtil::replace_chars(fmt::format("{}_nameless", saveFileProto.c_str()), illegalChars, '_');
    }
    else {
        saveFileBase = StrUtil::replace_chars(fmt::format("{}_{}", saveFileProto.c_str(), description.c_str()), illegalChars, '_');
    }

    uint32_t saveFileSuffix = 0;
    saveFile = saveFileBase;
    bool duplicate;
    do {
        duplicate = false;
        for (const auto& p : std::filesystem::directory_iterator(gearConfigDir)) {
            if (p.path().stem() == saveFile) {
                duplicate = true;
                saveFile = fmt::format("{}_{:02d}", saveFileBase.c_str(), saveFileSuffix++);
            }
        }
    } while (duplicate);

    GearInfo gearInfo(description, modelName, ENTITY::GET_ENTITY_MODEL(vehicle), licensePlate,
        topGear, driveMaxVel, ratios, loadType);
    GearInfo::SaveConfig(gearInfo, gearConfigDir + "\\" + saveFile + ".xml");
    UI::Notify(INFO, fmt::format("Saved as {}", saveFile));
}

void update_mainmenu() {
    menu.Title("Custom Gear Ratios");
    menu.Subtitle(std::string("~b~") + Constants::DisplayVersion);

    if (!currentVehicle || !ENTITY::DOES_ENTITY_EXIST(currentVehicle)) {
        menu.Option("No vehicle", { "Get in a vehicle to change its gear stats." });
        menu.MenuOption("Options", "optionsmenu", { "Change some preferences." });
        return;
    }

    auto extra = printGearStatus(currentVehicle, 255);
    menu.OptionPlus("Gearbox status", extra, nullptr, nullptr, nullptr, Util::GetFormattedVehicleModelName(currentVehicle));

    menu.MenuOption("Edit ratios", "ratiomenu");
    if (menu.MenuOption("Load ratios", "loadmenu")) {
        parseConfigs();
    }
    menu.MenuOption("Save ratios", "savemenu");

    menu.MenuOption("Options", "optionsmenu", { "Change some preferences." });
}

void update_ratiomenu() {
    menu.Title("Edit ratios");
    menu.Subtitle("");
    bool anyChanged = false;

    if (!currentVehicle || !ENTITY::DOES_ENTITY_EXIST(currentVehicle)) {
        menu.Option("No vehicle", { "Get in a vehicle to change its gear stats." });
        return;
    }

    uint8_t origNumGears = *reinterpret_cast<uint8_t *>(ext.GetHandlingPtr(currentVehicle) + hOffsets.nInitialDriveGears);
    uint32_t flags = *reinterpret_cast<uint32_t *>(ext.GetHandlingPtr(currentVehicle) + 0x128); // handling flags, b1604
    bool cvtFlag = flags & 0x00001000;

    std::string carName = Util::GetFormattedVehicleModelName(currentVehicle);

    // Change top gear
    if (cvtFlag) {
        menu.Option("Vehicle has CVT, can't be edited.", { "Get in a vehicle to change its gear stats." });
    }
    else {
        bool sel;
        uint8_t topGear = ext.GetTopGear(currentVehicle);
        menu.OptionPlus(fmt::format("Top gear: < {} >", topGear), {}, &sel,
            [&]() mutable { 
                incVal<uint8_t>(topGear, VehicleExtensions::GearsAvailable() - 1, 1);
                ext.SetTopGear(currentVehicle, topGear);
                anyChanged = true;
            },
            [&]() mutable {
                decVal<uint8_t>(topGear,  1, 1); 
                ext.SetTopGear(currentVehicle, topGear);
                anyChanged = true;
            },
            carName, 
            { "Press left to decrease top gear, right to increase top gear.",
               "~r~Warning: The default gearbox can't shift down from 9th gear!" });
        if (sel) {
            menu.OptionPlusPlus(printGearStatus(currentVehicle, 255), carName);
        }
    }

    // Change final drive
    {
        bool sel;
        const float min = 1.0f;
        const float max = 1000.0f;

        float driveMaxVel = ext.GetDriveMaxFlatVel(currentVehicle);
        bool triggered = menu.OptionPlus(fmt::format("Final drive max: < {:.1f} kph >", driveMaxVel * 3.6f), {}, &sel,
            [&]() mutable {
                incVal<float>(driveMaxVel, max, 0.36f);
                ext.SetDriveMaxFlatVel(currentVehicle, driveMaxVel);
                ext.SetInitialDriveMaxFlatVel(currentVehicle, driveMaxVel / 1.2f);
                anyChanged = true;
            },
            [&]() mutable {
                decVal<float>(driveMaxVel, min, 0.36f);
                ext.SetDriveMaxFlatVel(currentVehicle, driveMaxVel);
                ext.SetInitialDriveMaxFlatVel(currentVehicle, driveMaxVel / 1.2f);
                anyChanged = true;
            },
            carName, { "Select to type final drive in kph. Press left to decrease, right to increase." });
        if (sel) {
            menu.OptionPlusPlus(printGearStatus(currentVehicle, 255), carName);
        }

        float newSpeed = ext.GetDriveMaxFlatVel(currentVehicle) * 3.6f;
        if (triggered && GetKbEntryFloat(newSpeed)) {
            newSpeed = std::clamp(newSpeed, min, max);
            ext.SetDriveMaxFlatVel(currentVehicle, newSpeed / 3.6f);
            ext.SetInitialDriveMaxFlatVel(currentVehicle, (newSpeed / 3.6f) / 1.2f);
            anyChanged = true;
        }
    }

    uint8_t topGear = ext.GetTopGear(currentVehicle);

    if (topGear == 1 && settings.EnableCVT) {
        menu.FloatOption("Low range ratio", settings.CVT.LowRatio, 0.0f, 10.0f, 0.05f);
        menu.FloatOption("High range ratio", settings.CVT.HighRatio, 0.0f, 10.0f, 0.05f);
        menu.FloatOption("Factor", settings.CVT.Factor, 0.0f, 10.0f, 0.05f);
    }
    else {
        for (uint8_t gear = 0; gear <= topGear; ++gear) {
            bool sel = false;
            float min = 0.10f;
            float max = 10.0f;

            if (gear == 0) {
                min = -10.0f;
                max = -0.10f;
            }

            bool triggered = menu.OptionPlus(fmt::format("Gear {}", gear), {}, &sel,
                [&]() mutable {
                    incVal(*reinterpret_cast<float*>(ext.GetGearRatioPtr(currentVehicle, gear)), max, 0.01f);
                    anyChanged = true;
                },
                [&]() mutable {
                    decVal(*reinterpret_cast<float*>(ext.GetGearRatioPtr(currentVehicle, gear)), min, 0.01f);
                    anyChanged = true;
                },
                    carName, { "Select to type gear ratio. Press left to decrease, right to increase." });
            if (sel) {
                menu.OptionPlusPlus(printGearStatus(currentVehicle, gear), carName);
            }

            float newRatio = *ext.GetGearRatioPtr(currentVehicle, gear);
            if (triggered && GetKbEntryFloat(newRatio)) {
                newRatio = std::clamp(newRatio, min, max);
                *reinterpret_cast<float*>(ext.GetGearRatioPtr(currentVehicle, gear)) = newRatio;
                anyChanged = true;
            }
        }
    }

    // Apply a default curve
    // Curve based on https://www.desmos.com/calculator/2fncxrhrvi, where points are the 6 gears.
    {
        bool sel;
        bool changed = menu.OptionPlus(fmt::format("Optimized defaults"), {}, &sel,
            nullptr, nullptr, carName, { "Set ratios based on an approximation of the default curve.",
                "~r~Overwrites~w~ the ratios visible on the right!" });
        if (changed) {
            *reinterpret_cast<float*>(ext.GetGearRatioPtr(currentVehicle, 0)) = -3.33f;
            for (uint8_t gear = 1; gear <= topGear; ++gear) {
                double gearRatio = map(static_cast<double>(gear), 1.0, static_cast<double>(topGear), 0.0, 1.0);
                float magicVal = static_cast<float>(2.5 * pow(0.018, gearRatio) + 0.83333333);
                *reinterpret_cast<float*>(ext.GetGearRatioPtr(currentVehicle, gear)) = magicVal;
            }
            anyChanged = true;
        }
        if (sel) {
            menu.OptionPlusPlus(printGearStatus(currentVehicle, 255), carName);
        }
    }

    if (anyChanged) {
        auto currCfgCombo = std::find_if(currentConfigs.begin(), currentConfigs.end(), [=](const auto& cfg) {return cfg.first == currentVehicle; });
        
        if (currCfgCombo != currentConfigs.end()) {
            auto& currentConfig = currCfgCombo->second;
            currentConfig.TopGear = ext.GetTopGear(currentVehicle);
            currentConfig.DriveMaxVel = ext.GetDriveMaxFlatVel(currentVehicle);
            currentConfig.Ratios = ext.GetGearRatios(currentVehicle);
        }
        else {
            UI::Notify(INFO, "Something messed up, check log.");
            logger.Write(ERROR, "Could not find currvehicle {} in list of vehicles?", currentVehicle);
        }
    }
}

void update_loadmenu() {
    menu.Title("Load ratios");
    menu.Subtitle("");

    if (!currentVehicle || !ENTITY::DOES_ENTITY_EXIST(currentVehicle)) {
        menu.Option("No vehicle", { "Get in a vehicle to change its gear stats." });
        return;
    }

    if (gearConfigs.empty()) {
        menu.Option("No saved ratios");
    }


    for (auto& config : gearConfigs) {
        bool selected;
        std::string modelName = Util::GetFormattedModelName(
            MISC::GET_HASH_KEY(config.ModelName.c_str()));

        if (modelName == "CARNOTFOUND") {
            modelName = Util::GetFormattedModelName(config.ModelHash);
        }

        std::string optionName = fmt::format("{} - {} gears - {:.0f} kph", 
            modelName.c_str(), config.TopGear, 
            3.6f * config.DriveMaxVel / config.Ratios[config.TopGear]);
        
        std::vector<std::string> extras = { "Press Enter/Accept to load." };

        if (config.MarkedForDeletion) {
            extras.emplace_back("~r~Marked for deletion. ~s~Press Right again to restore."
                " File will be removed on menu exit!");
            optionName = fmt::format("~r~{}", optionName.c_str());
        }
        else {
            extras.emplace_back("Press Right to mark for deletion.");
        }

        if (menu.OptionPlus(optionName, std::vector<std::string>(), &selected,
                [&]() mutable { config.MarkedForDeletion = !config.MarkedForDeletion; },
                nullptr, modelName, extras)) {
            applyConfig(config, currentVehicle, true, true);
        }
        if (selected) {
            menu.OptionPlusPlus(printInfo(config), modelName);
        }
    }
}

void update_savemenu() {
    menu.Title("Save ratios");
    menu.Subtitle("");

    if (!currentVehicle || !ENTITY::DOES_ENTITY_EXIST(currentVehicle)) {
        menu.Option("No vehicle", { "Get in a vehicle to change its gear stats." });
        return;
    }

    if (menu.Option("Save as autoload", 
        { "Save current gear setup for model and license plate.",
        "It will load automatically when entering a car "
            "with the same model and plate text."})) {
        promptSave(currentVehicle, LoadType::Plate);
    }

    if (menu.Option("Save as generic autoload",
        { "Save current gear setup with generic autoload."
            "Overridden by plate-specific autoload." })) {
        promptSave(currentVehicle, LoadType::Model);
    }

    if (menu.Option("Save as generic",
        { "Save current gear setup without autoload." })) {
        promptSave(currentVehicle, LoadType::None);
    }
}

void update_optionsmenu() {
    menu.Title("Options");
    menu.Subtitle("");

    menu.BoolOption("Autoload ratios (License plate)", settings.AutoLoad,
        { "Load gear ratio mapping automatically when getting into a vehicle"
            " that matches model and license plate." });
    menu.BoolOption("Autoload ratios (Generic)", settings.AutoLoadGeneric,
        { "Load gear ratio mapping automatically when getting into a vehicle"
            " that matches model. Overridden by plate." });
    menu.BoolOption("Override game ratio changes", settings.RestoreRatios,
        { "Restores user-set ratios when the game changes them,"
            " for example gearbox upgrades in LSC." });
    menu.BoolOption("Autoload notifications", settings.AutoNotify,
        { "Show a notification when autoload applied a preset." });
    menu.BoolOption("Enable CVT when 1 gear", settings.EnableCVT,
        { "Enable custom CVT when setting number of gears to 1 in a car that doesn't come with CVT." });
    menu.BoolOption("Enable for NPCs", settings.EnableNPC,
        { "Enables custom gear ratios for NPCs. Autoload configuration is used to select ratios." });
}

void update_menu() {
    menu.CheckKeys();

    /* mainmenu */
    if (menu.CurrentMenu("mainmenu")) { update_mainmenu(); }

    if (menu.CurrentMenu("ratiomenu")) { update_ratiomenu(); }

    if (menu.CurrentMenu("loadmenu")) { update_loadmenu(); }

    if (menu.CurrentMenu("savemenu")) { update_savemenu(); }
    
    if (menu.CurrentMenu("optionsmenu")) { update_optionsmenu(); }

    menu.EndMenu();
}