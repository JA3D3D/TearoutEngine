# Installing TEAROUT ENGINE on a Mac (Ableton Live 11)

## Easiest: the installer (.pkg)

1. Double-click `TEAROUT_ENGINE_1.0_macOS.pkg`. It puts:
   - the VST3 in `/Library/Audio/Plug-Ins/VST3`
   - the AU in `/Library/Audio/Plug-Ins/Components`
   - the app in `/Applications`
2. The installer isn't notarized by Apple, so macOS may refuse to open it the first time. Either:
   - right-click it and choose **Open**, then confirm, or
   - go to **System Settings → Privacy & Security** and click **Open Anyway**.
3. In Live 11, go to **Preferences → Plug-Ins**. Turn on **Use Audio Units** and/or **Use VST3 Plug-In System Folders**, then **Rescan**.

## Manual install (from the zip)

1. Unzip `TEAROUT_ENGINE_macOS.zip`.
2. Copy the plugins into your plug-in folders. In Finder, use *Go → Go to Folder…*:
   - `TEAROUT ENGINE.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `TEAROUT ENGINE.component` → `~/Library/Audio/Plug-Ins/Components/`
3. **Let macOS open it.** These builds are signed ad hoc but not notarized by Apple, so macOS quarantines them. Open **Terminal** and paste:

   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"TEAROUT ENGINE.vst3"
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"TEAROUT ENGINE.component"
   ```
4. In Live 11: **Live → Preferences → Plug-Ins**. Turn on **Use Audio Units** and/or **Use VST3 Plug-In System Folders**, then press **Rescan**.
5. In Live's browser, open **Plug-Ins → TEAROUT → TEAROUT ENGINE** and drag it onto a **MIDI track**. It's an instrument.

`TEAROUT ENGINE.app` is a standalone version. Use it to try the generator without opening Live, and drag MIDI out of it into anything.

**Apple Silicon and Intel:** the build is universal, so it runs natively on both.
