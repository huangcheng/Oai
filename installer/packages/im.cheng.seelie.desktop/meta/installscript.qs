// Install script for Seelie Desktop Pet.
// Installer is English-only for v1.0.0 — Chinese translations were removed
// because IFW 4.11 ships no zh_CN chrome translations, which produced an
// inconsistent UI (English wizard chrome + Chinese package metadata) on
// zh_CN system locales. Re-add a picker in a future version once we can
// ship a complete zh_CN chrome translation.

function Component()
{
    installer.installationFinished.connect(this, Component.prototype.onInstallationFinishedPage);
    installer.finishButtonClicked.connect(this, Component.prototype.onFinishButtonClicked);
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        var exe        = "@TargetDir@/Seelie.exe";
        var maintainer = "@TargetDir@/maintenancetool.exe";
        var appLabel   = "Seelie Desktop Pet";
        var uninstLbl  = "Uninstall Seelie";

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
            + "Name=Seelie Desktop Pet\n"
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
            installer.addWizardPageItem(component, "LaunchCheckBoxForm", QInstaller.InstallationFinished);
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
