import sys
import re

with open("lib/CanManager/CanManager.cpp", "r") as f:
    content = f.read()

replacement = """    // Bu mutex bugun tek task tarafindan kullaniliyor; gercek task-arasi veri yolu queue + std::atomic'tir. portMAX_DELAY, watchdog panigi kapaliyken kurtarilamaz kilitlenme kaynagidir. (AKS-21)
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGE(TAG, "s_mutex timeout!");
"""

def replace_match(m):
    # m.group(0) is the match. We need to look a bit backwards to know the context.
    # But wait, python regex sub with a function is easier.
    pass

# Actually let's just do it line by line
lines = content.split('\n')
out_lines = []
for i, line in enumerate(lines):
    if "xSemaphoreTake(s_mutex, portMAX_DELAY);" in line:
        # Determine the return statement based on the function context
        # We can look up to find the function signature or variable declarations.
        ret_val = "return;"
        # check recent lines
        for j in range(i-1, i-20, -1):
            if "MotorStatus CAN_statusCopy" in lines[j]:
                ret_val = "return CAN_statusCopy;"
                break
            elif "TelemetryData CAN_telemetryCopy" in lines[j]:
                ret_val = "return CAN_telemetryCopy;"
                break
        
        out_lines.append("    // Bu mutex bugun tek task tarafindan kullaniliyor; gercek task-arasi veri yolu queue + std::atomic'tir. portMAX_DELAY, watchdog panigi kapaliyken kurtarilamaz kilitlenme kaynagidir. (AKS-21)")
        out_lines.append("    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {")
        out_lines.append('        ESP_LOGE(TAG, "s_mutex timeout!");')
        out_lines.append(f"        {ret_val}")
        out_lines.append("    }")
    else:
        out_lines.append(line)

with open("lib/CanManager/CanManager.cpp", "w") as f:
    f.write('\n'.join(out_lines))
