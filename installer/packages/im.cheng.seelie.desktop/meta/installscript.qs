// Install script for Seelie Desktop Pet.
// User-visible strings go through qsTr(); translations live next to this
// file as <lang>.qm and are auto-loaded by Qt IFW from the system locale
// or the --lang CLI flag (installerbase --lang zh_CN).

function Component()
{
    // The wizard skeleton doesn't exist yet at constructor time, so defer
    // page registration to guiElementsReady(). addWizardPage (NOT
    // addWizardPageItem — that inserts INTO an existing page) is the API
    // that creates a new page in the wizard.
    if (installer.isInstaller()) {
        installer.guiElementsReady.connect(this,
            Component.prototype.onGuiElementsReady);
    }

    installer.installationFinished.connect(this,
        Component.prototype.onInstallationFinishedPage);
    installer.finishButtonClicked.connect(this,
        Component.prototype.onFinishButtonClicked);

    component.languageRestartTriggered = false;
}

Component.prototype.onGuiElementsReady = function()
{
    // Insert the language picker BEFORE the standard Introduction page so it
    // is the very first thing the user sees. Skip when the user already
    // restarted with --lang (UILanguage will be set explicitly then).
    installer.addWizardPage(component, "LanguageSelectorForm",
                            QInstaller.Introduction);

    var form = component.userInterface("LanguageSelectorForm");
    if (!form || !form.languageCombo) return;

    form.languageCombo.addItem("English", "en");
    form.languageCombo.addItem("中文 (简体)", "zh_CN");

    // Pre-select the active language so the combo reflects the current state.
    // installer.value("UILanguage") returns the locale code IFW resolved from
    // --lang flag or system locale (e.g. "en", "zh_CN").
    var current = String(installer.value("UILanguage", "en"));
    var prefix = current.split("_")[0];
    for (var i = 0; i < form.languageCombo.count; ++i) {
        var code = String(form.languageCombo.itemData(i));
        if (code === current || code.split("_")[0] === prefix) {
            form.languageCombo.currentIndex = i;
            break;
        }
    }

    form.languageCombo["currentIndexChanged(int)"].connect(this,
        Component.prototype.onLanguageChanged);
};

Component.prototype.onLanguageChanged = function(index)
{
    if (component.languageRestartTriggered) return; // guard double-fire

    var form = component.userInterface("LanguageSelectorForm");
    if (!form || !form.languageCombo) return;
    var newLang = String(form.languageCombo.itemData(index));
    var currentLang = String(installer.value("UILanguage", "en"));
    if (newLang === currentLang) return;

    // Restart the installer with --lang <code>. IFW's installerbase reads the
    // flag during startup; this re-loads chrome + our package qsTr() strings
    // in the chosen language. No in-process reload mechanism exists.
    component.languageRestartTriggered = true;

    var argv = installer.value("installerbase");  // resolved by IFW
    if (!argv) argv = installer.value("InstallerFilePath", "");
    if (!argv) {
        console.log("Cannot resolve installer binary for restart");
        component.languageRestartTriggered = false;
        return;
    }

    try {
        installer.executeDetached(argv, ["--lang", newLang], "");
        installer.setCanceled();
    } catch (e) {
        console.log("Language restart failed: " + e);
        component.languageRestartTriggered = false;
    }
};

// --- Original install logic -------------------------------------------------

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        var exe        = "@TargetDir@/Seelie.exe";
        var maintainer = "@TargetDir@/maintenancetool.exe";
        var appLabel   = qsTr("Seelie Desktop Pet");
        var uninstLbl  = qsTr("Uninstall Seelie");

        component.addOperation("CreateShortcut",
            exe,
            "@DesktopDir@/Seelie.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=" + exe,
            "description=" + appLabel);

        component.addOperation("CreateShortcut",
            exe,
            "@StartMenuDir@/Seelie.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=" + exe,
            "description=" + appLabel);

        component.addOperation("CreateShortcut",
            maintainer,
            "@StartMenuDir@/" + uninstLbl + ".lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=" + maintainer,
            "description=" + uninstLbl);
    } else if (systemInfo.productType === "macos" || systemInfo.kernelType === "darwin") {
        // The macOS bundle goes into Applications via @TargetDir@/Seelie.app;
        // no extra desktop entries needed.
    } else {
        // Linux: drop a .desktop entry into the user's applications dir.
        component.addOperation("CreateDesktopEntry",
            "Seelie.desktop",
            "Type=Application\n"
            + "Name=" + qsTr("Seelie Desktop Pet") + "\n"
            + "Exec=@TargetDir@/Seelie\n"
            + "Icon=@TargetDir@/seelie.png\n"
            + "Terminal=false\n"
            + "Categories=Utility;\n");
    }
};

Component.prototype.onInstallationFinishedPage = function()
{
    try {
        if (installer.isInstaller() && installer.status === QInstaller.Success) {
            installer.addWizardPageItem(component, "LaunchCheckBoxForm",
                                        QInstaller.InstallationFinished);
            // The .ui file hard-codes English; override with qsTr() so the
            // string is picked up by seelie_installer_<lang>.qm translations.
            var form = component.userInterface("LaunchCheckBoxForm");
            if (form && form.launchCheckBox) {
                form.launchCheckBox.setText(qsTr("Launch Seelie now"));
            }
        }
    } catch (e) {
        console.log(e);
    }
};

Component.prototype.onFinishButtonClicked = function()
{
    try {
        if (!installer.isInstaller() || installer.status !== QInstaller.Success) return;

        var form = component.userInterface("LaunchCheckBoxForm");
        if (form && form.launchCheckBox && form.launchCheckBox.checked) {
            if (systemInfo.productType === "windows") {
                installer.executeDetached("@TargetDir@/Seelie.exe");
            } else if (systemInfo.kernelType === "darwin") {
                installer.executeDetached("open", ["@TargetDir@/Seelie.app"]);
            } else {
                installer.executeDetached("@TargetDir@/Seelie");
            }
        }
    } catch (e) {
        console.log(e);
    }
};
