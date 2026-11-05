import re

# Read generated masks
with open("scratch/new_masks.h", "r", encoding="utf-8") as f:
    new_masks_text = f.read()

new_icon_names = [
    "caloryGoalAchievedIcon",
    "directionIcon",
    "fontIcon",
    "hotSpotIcon",
    "languageIcon",
    "locationPinIcon",
    "lockIcon",
    "uploadIcon",
    "warningIcon"
]

def update_file(path):
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. Update enum class CustomIcon
    for name in new_icon_names:
        if name not in content:
            content = content.replace(
                "satelliteIconLarge };",
                f"satelliteIconLarge, {', '.join(new_icon_names)} }};"
            )
            break

    # 2. Append new masks before customMasks
    if "caloryGoalAchievedIconMask" not in content:
        insert_marker = "static const uint8_t *const customMasks[] = {"
        content = content.replace(insert_marker, new_masks_text + "\n" + insert_marker)

        # 3. Update customMasks[] array
        old_masks_end = "historyIconMask,satelliteIconLargeMask};"
        new_masks_entries = ", ".join(f"{name}Mask" for name in new_icon_names)
        content = content.replace(old_masks_end, f"historyIconMask,satelliteIconLargeMask, {new_masks_entries}}};")

        # 4. Update customWidths[] and customHeights[]
        old_widths_end = "16,32};"
        new_dims = ", " + ", ".join(["16"] * len(new_icon_names)) + "};"
        # Be careful: customWidths has 16,32}; and customHeights has 16,32};
        # Replace both occurrences
        content = content.replace(old_widths_end, f"16,32{new_dims}")

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print("Updated", path)

update_file("firmware/src/custom_icon_masks.h")
update_file("schematikIO-Export/src/custom_icon_masks.h")
