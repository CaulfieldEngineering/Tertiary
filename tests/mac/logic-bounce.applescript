-- [Codex 2026-10-30] Future System Events bounce adapter, bounded and scoped to a copied harness.
-- Harness contract: cycle locators, PCM/WAVE/48 kHz/32-bit float/offline are saved beforehand.
-- Joe performs first launch. Unknown windows cause an error and a screenshot in the Python wrapper.
on run argv
    set appPath to item 1 of argv
    set projectPath to item 2 of argv
    set outputPath to item 3 of argv
    set appName to do shell script "/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' " & quoted form of (appPath & "/Contents/Info.plist")
    with timeout of 210 seconds
        try
            do shell script "/usr/bin/open -a " & quoted form of appPath & " " & quoted form of projectPath
            set deadline to (current date) + 45
            tell application "System Events"
                repeat until exists process appName
                    if (current date) > deadline then error "Logic did not start"
                    delay 1
                end repeat
                tell process appName
                    set frontmost to true
                    repeat until (count of windows) > 0
                        if (current date) > deadline then error "No Logic project window"
                        delay 1
                    end repeat
                    delay 3
                    if name of front window does not contain "harness" then error "Unexpected Logic window before bounce"
                    if (count of sheets of front window) > 0 then error "Unexpected sheet before bounce"
                    keystroke "b" using command down
                    delay 3
                    if not (exists button "Bounce" of front window) then error "Expected Bounce dialog and button"
                    click button "Bounce" of front window
                    delay 2
                    keystroke "g" using {command down, shift down}
                    delay 1
                    keystroke outputPath
                    key code 36
                    delay 1
                    keystroke "a" using command down
                    keystroke "harness"
                    key code 36
                end tell
            end tell
            set deadline to (current date) + 120
            repeat
                try
                    do shell script "test -s " & quoted form of (outputPath & "/harness.wav")
                    exit repeat
                end try
                if (current date) > deadline then error "Logic bounce not written within two minutes"
                delay 1
            end repeat
            delay 3
            tell application appPath to quit saving no
        on error messageText number errorNumber
            do shell script "/usr/sbin/screencapture -x " & quoted form of (outputPath & "/logic-failure.png")
            try
                tell application appPath to quit saving no
            end try
            error messageText number errorNumber
        end try
    end timeout
end run
