#!/bin/sh

RED_SECBOOT=""; RED_SBFLAG=""; RED_BOOTFAIL=""; RED_FLASHTYPE=""; RED_AUTH=""

redline_read(){
    RED_SECBOOT=""; RED_SBFLAG=""; RED_BOOTFAIL=""; RED_FLASHTYPE=""; RED_AUTH=""

    if has fw_printenv; then
        _e="$(fw_printenv secboot sb_flag boot_fail_count flash_type 2>/dev/null)"
        RED_SECBOOT="$(printf '%s\n' "$_e"  | sed -n 's/^secboot=//p')"
        RED_SBFLAG="$(printf '%s\n'  "$_e"  | sed -n 's/^sb_flag=//p')"
        RED_BOOTFAIL="$(printf '%s\n' "$_e" | sed -n 's/^boot_fail_count=//p')"
        RED_FLASHTYPE="$(printf '%s\n' "$_e"| sed -n 's/^flash_type=//p')"
    fi

    if [ -r "$QFPR0M_AUTH" ]; then
        RED_AUTH="$(cat "$QFPR0M_AUTH" 2>/dev/null)"
    else
        RED_AUTH=""
    fi
}

redline_ok(){
    [ "$RED_SECBOOT" = "2" ] && [ "$RED_FLASHTYPE" = "5" ]
}

redline_summary(){
    printf 'secboot=%s sb_flag=%s boot_fail_count=%s flash_type=%s authenticate=%s' \
        "${RED_SECBOOT:-?}" "${RED_SBFLAG:-?}" "${RED_BOOTFAIL:-?}" \
        "${RED_FLASHTYPE:-?}" "${RED_AUTH:-unavailable}"
}

redline_json(){
    printf '{"secboot":"%s","sb_flag":"%s","boot_fail_count":"%s","flash_type":"%s","authenticate":"%s","ok":%s}' \
        "$(json_str "$RED_SECBOOT")" "$(json_str "$RED_SBFLAG")" \
        "$(json_str "$RED_BOOTFAIL")" "$(json_str "$RED_FLASHTYPE")" \
        "$(json_str "$RED_AUTH")" \
        "$(redline_ok && echo true || echo false)"
}
