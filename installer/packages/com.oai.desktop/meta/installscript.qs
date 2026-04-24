// Install script for Orai Desktop Pet

function Component()
{
    // Connect to installation finished signal
    installer.installationFinished.connect(this, Component.prototype.installationFinishedPageIsShown);
    installer.finishButtonClicked.connect(this, Component.prototype.installationFinished);
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Create desktop shortcut
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/Orai.exe",
            "@DesktopDir@/Orai.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/Orai.exe",
            "description=Orai Desktop Pet"
        );

        // Create start menu shortcut
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/Orai.exe",
            "@StartMenuDir@/Orai.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/Orai.exe",
            "description=Orai Desktop Pet"
        );

        // Create uninstaller shortcut
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/maintenancetool.exe",
            "@StartMenuDir@/Uninstall Orai.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/maintenancetool.exe",
            "description=Uninstall Orai"
        );
    }
}

Component.prototype.installationFinishedPageIsShown = function()
{
    // Show "Launch Orai" checkbox on finish page
    try {
        if (installer.isInstaller() && installer.status == QInstaller.Success) {
            installer.addWizardPageItem(component, "LaunchCheckBox", QInstaller.InstallationFinished);
        }
    } catch(e) {
        console.log(e);
    }
}

Component.prototype.installationFinished = function()
{
    // Launch Orai if checkbox is checked
    try {
        if (installer.isInstaller() && installer.status == QInstaller.Success) {
            var runOrai = component.userInterface("LaunchCheckBox").checked;
            if (runOrai) {
                installer.executeDetached("@TargetDir@/Orai.exe");
            }
        }
    } catch(e) {
        console.log(e);
    }
}
