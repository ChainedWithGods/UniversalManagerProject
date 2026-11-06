print("[Avalonium] [Overlay] Universal Live Metrics Overlay loaded.")
print("[Avalonium] [Overlay]   F6 — Toggle Live Metrics HUD")

RegisterKeyBind("F6", function()
    local visible = ToggleMetrics()
    if visible then
        print("[Avalonium] [Overlay] Live metrics HUD visible")
    else
        print("[Avalonium] [Overlay] Live metrics HUD hidden")
    end
end)
