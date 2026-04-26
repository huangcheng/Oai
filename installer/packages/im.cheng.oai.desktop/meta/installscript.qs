// Install script for Oai Desktop Pet.
// User-visible strings go through qsTr(); translations live next to this
// file as <lang>.qm and are auto-loaded by Qt IFW from the system locale.

function Component()
{
    // Pin install location to @ApplicationsDir@/Oai (declared in config.xml).
    // Hiding TargetDirectory removes the folder picker page entirely; users
    // can't relocate the install. Admin elevation is still triggered on
    // Windows because Program Files requires it.
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);

    installer.installationFinished.connect(this, Component.prototype.onInstallationFinishedPage);
    installer.finishButtonClicked.connect(this, Component.prototype.onFinishButtonClicked);
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        var exe        = "@TargetDir@/Oai.exe";
        var maintainer = "@TargetDir@/maintenancetool.exe";
        var appLabel   = qsTr("Oai Desktop Pet");
        var uninstLbl  = qsTr("Uninstall Oai");

        component.addOperation("CreateShortcut",
            exe,
            "@DesktopDir@/Oai.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=" + exe,
            "description=" + appLabel);

        component.addOperation("CreateShortcut",
            exe,
            "@StartMenuDir@/Oai.lnk",
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
        // The macOS bundle goes into Applications via @TargetDir@/Oai.app;
        // no extra desktop entries needed.
    } else {
        // Linux: drop a .desktop entry into the user's applications dir.
        component.addOperation("CreateDesktopEntry",
            "Oai.desktop",
            "Type=Application\n"
            + "Name=" + qsTr("Oai Desktop Pet") + "\n"
            + "Exec=@TargetDir@/Oai\n"
            + "Icon=@TargetDir@/oai.png\n"
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
                installer.executeDetached("@TargetDir@/Oai.exe");
            } else if (systemInfo.kernelType === "darwin") {
                installer.executeDetached("open", ["@TargetDir@/Oai.app"]);
            } else {
                installer.executeDetached("@TargetDir@/Oai");
            }
        }
    } catch (e) {
        console.log(e);
    }
};
