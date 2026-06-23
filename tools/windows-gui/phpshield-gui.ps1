param(
    [string]$PhpPath = "php",
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

Add-Type -AssemblyName PresentationFramework
Add-Type -AssemblyName System.Windows.Forms

$ErrorActionPreference = "Stop"
$CliPath = Join-Path $ProjectRoot "bin\phpshield"

function New-Label($Text, $Row) {
    $label = New-Object Windows.Controls.Label
    $label.Content = $Text
    $label.Margin = "8,4,8,4"
    [Windows.Controls.Grid]::SetRow($label, $Row)
    [Windows.Controls.Grid]::SetColumn($label, 0)
    return $label
}

function New-TextBox($Row, $Text = "") {
    $box = New-Object Windows.Controls.TextBox
    $box.Text = $Text
    $box.Margin = "8,4,4,4"
    $box.MinWidth = 520
    [Windows.Controls.Grid]::SetRow($box, $Row)
    [Windows.Controls.Grid]::SetColumn($box, 1)
    return $box
}

function New-Button($Text, $Row, $Column = 2) {
    $button = New-Object Windows.Controls.Button
    $button.Content = $Text
    $button.Margin = "4,4,8,4"
    $button.Padding = "12,4,12,4"
    [Windows.Controls.Grid]::SetRow($button, $Row)
    [Windows.Controls.Grid]::SetColumn($button, $Column)
    return $button
}

function Select-Folder($Target) {
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.SelectedPath = $Target.Text
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $Target.Text = $dialog.SelectedPath
    }
}

function Select-File($Target, $Filter) {
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Filter = $Filter
    if ($Target.Text -and (Test-Path $Target.Text)) {
        $dialog.InitialDirectory = Split-Path $Target.Text -Parent
    }
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $Target.Text = $dialog.FileName
    }
}

function Save-File($Target, $Filter) {
    $dialog = New-Object System.Windows.Forms.SaveFileDialog
    $dialog.Filter = $Filter
    if ($Target.Text) {
        $dialog.FileName = Split-Path $Target.Text -Leaf
    }
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $Target.Text = $dialog.FileName
    }
}

function Invoke-PhpShield($Arguments) {
    $script:OutputBox.AppendText("`r`n> php $CliPath $($Arguments -join ' ')`r`n")
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $PhpPath
    $psi.WorkingDirectory = $ProjectRoot
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.ArgumentList.Add($CliPath)
    foreach ($arg in $Arguments) {
        $psi.ArgumentList.Add($arg)
    }
    $process = [System.Diagnostics.Process]::Start($psi)
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($stdout) { $script:OutputBox.AppendText($stdout) }
    if ($stderr) { $script:OutputBox.AppendText($stderr) }
    $script:OutputBox.AppendText("exit code: $($process.ExitCode)`r`n")
    $script:OutputBox.ScrollToEnd()
}

$window = New-Object Windows.Window
$window.Title = "PHPShield Encoder"
$window.Width = 980
$window.Height = 760
$window.WindowStartupLocation = "CenterScreen"

$root = New-Object Windows.Controls.DockPanel
$window.Content = $root

$tabs = New-Object Windows.Controls.TabControl
$tabs.Margin = "10"
[Windows.Controls.DockPanel]::SetDock($tabs, "Top")
$root.Children.Add($tabs) | Out-Null

$encodeTab = New-Object Windows.Controls.TabItem
$encodeTab.Header = "Encode"
$encodeGrid = New-Object Windows.Controls.Grid
$encodeGrid.Margin = "8"
$encodeTab.Content = $encodeGrid
$tabs.Items.Add($encodeTab) | Out-Null

0..8 | ForEach-Object {
    $row = New-Object Windows.Controls.RowDefinition
    $row.Height = "Auto"
    $encodeGrid.RowDefinitions.Add($row)
}
("Auto","*","Auto") | ForEach-Object {
    $col = New-Object Windows.Controls.ColumnDefinition
    $col.Width = $_
    $encodeGrid.ColumnDefinitions.Add($col)
}

$sourceBox = New-TextBox 0
$outputBox = New-TextBox 1
$keyBox = New-TextBox 2 (Join-Path $ProjectRoot "tmp\master.key")
$pubKeyBox = New-TextBox 3 (Join-Path $ProjectRoot "tmp\license.ed25519.pub")
$productBox = New-TextBox 4 "phpshield-demo"
$bundleBox = New-TextBox 5 "app.pshield"
$profileBox = New-TextBox 6 "default"
$requireLicense = New-Object Windows.Controls.CheckBox
$requireLicense.Content = "Require license"
$requireLicense.Margin = "8,4,8,4"
[Windows.Controls.Grid]::SetRow($requireLicense, 7)
[Windows.Controls.Grid]::SetColumn($requireLicense, 1)
$production = New-Object Windows.Controls.CheckBox
$production.Content = "Production bundle"
$production.Margin = "8,4,8,4"
[Windows.Controls.Grid]::SetRow($production, 8)
[Windows.Controls.Grid]::SetColumn($production, 1)

$encodeGrid.Children.Add((New-Label "Source" 0)) | Out-Null
$encodeGrid.Children.Add($sourceBox) | Out-Null
$browseSource = New-Button "Browse" 0
$browseSource.Add_Click({ Select-Folder $sourceBox })
$encodeGrid.Children.Add($browseSource) | Out-Null
$encodeGrid.Children.Add((New-Label "Output" 1)) | Out-Null
$encodeGrid.Children.Add($outputBox) | Out-Null
$browseOutput = New-Button "Browse" 1
$browseOutput.Add_Click({ Select-Folder $outputBox })
$encodeGrid.Children.Add($browseOutput) | Out-Null
$encodeGrid.Children.Add((New-Label "Master key" 2)) | Out-Null
$encodeGrid.Children.Add($keyBox) | Out-Null
$browseKey = New-Button "Browse" 2
$browseKey.Add_Click({ Select-File $keyBox "Key files (*.key)|*.key|All files (*.*)|*.*" })
$encodeGrid.Children.Add($browseKey) | Out-Null
$encodeGrid.Children.Add((New-Label "License public key" 3)) | Out-Null
$encodeGrid.Children.Add($pubKeyBox) | Out-Null
$browsePub = New-Button "Browse" 3
$browsePub.Add_Click({ Select-File $pubKeyBox "Public keys (*.pub)|*.pub|All files (*.*)|*.*" })
$encodeGrid.Children.Add($browsePub) | Out-Null
$encodeGrid.Children.Add((New-Label "Product ID" 4)) | Out-Null
$encodeGrid.Children.Add($productBox) | Out-Null
$encodeGrid.Children.Add((New-Label "Bundle name" 5)) | Out-Null
$encodeGrid.Children.Add($bundleBox) | Out-Null
$encodeGrid.Children.Add((New-Label "Profile" 6)) | Out-Null
$encodeGrid.Children.Add($profileBox) | Out-Null
$encodeGrid.Children.Add($requireLicense) | Out-Null
$encodeGrid.Children.Add($production) | Out-Null
$runEncode = New-Button "Encode" 8
$runEncode.Add_Click({
    $args = @("encode", $sourceBox.Text, $outputBox.Text, "--key-file", $keyBox.Text, "--product-id", $productBox.Text, "--bundle-name", $bundleBox.Text, "--profile", $profileBox.Text)
    if ($requireLicense.IsChecked) { $args += "--require-license" }
    if ($pubKeyBox.Text) { $args += @("--license-public-key", $pubKeyBox.Text) }
    if ($production.IsChecked) { $args += "--production" }
    Invoke-PhpShield $args
})
$encodeGrid.Children.Add($runEncode) | Out-Null

$licenseTab = New-Object Windows.Controls.TabItem
$licenseTab.Header = "License"
$licenseGrid = New-Object Windows.Controls.Grid
$licenseGrid.Margin = "8"
$licenseTab.Content = $licenseGrid
$tabs.Items.Add($licenseTab) | Out-Null

0..7 | ForEach-Object {
    $row = New-Object Windows.Controls.RowDefinition
    $row.Height = "Auto"
    $licenseGrid.RowDefinitions.Add($row)
}
("Auto","*","Auto") | ForEach-Object {
    $col = New-Object Windows.Controls.ColumnDefinition
    $col.Width = $_
    $licenseGrid.ColumnDefinitions.Add($col)
}

$licensePrivateBox = New-TextBox 0 (Join-Path $ProjectRoot "tmp\license.ed25519.sk")
$licenseOutBox = New-TextBox 1 (Join-Path $ProjectRoot "tmp\license.pslic")
$licenseProductBox = New-TextBox 2 "phpshield-demo"
$customerBox = New-TextBox 3 "customer-001"
$expiresBox = New-TextBox 4 ([DateTime]::UtcNow.AddDays(365).ToString("yyyy-MM-ddTHH:mm:ssZ"))
$domainsBox = New-TextBox 5
$machineBox = New-TextBox 6

$licenseGrid.Children.Add((New-Label "Private key" 0)) | Out-Null
$licenseGrid.Children.Add($licensePrivateBox) | Out-Null
$licensePrivateBrowse = New-Button "Browse" 0
$licensePrivateBrowse.Add_Click({ Select-File $licensePrivateBox "Secret keys (*.sk)|*.sk|All files (*.*)|*.*" })
$licenseGrid.Children.Add($licensePrivateBrowse) | Out-Null
$licenseGrid.Children.Add((New-Label "License file" 1)) | Out-Null
$licenseGrid.Children.Add($licenseOutBox) | Out-Null
$licenseSave = New-Button "Save As" 1
$licenseSave.Add_Click({ Save-File $licenseOutBox "PHPShield license (*.pslic)|*.pslic|All files (*.*)|*.*" })
$licenseGrid.Children.Add($licenseSave) | Out-Null
$licenseGrid.Children.Add((New-Label "Product ID" 2)) | Out-Null
$licenseGrid.Children.Add($licenseProductBox) | Out-Null
$licenseGrid.Children.Add((New-Label "Customer ID" 3)) | Out-Null
$licenseGrid.Children.Add($customerBox) | Out-Null
$licenseGrid.Children.Add((New-Label "Expires UTC" 4)) | Out-Null
$licenseGrid.Children.Add($expiresBox) | Out-Null
$licenseGrid.Children.Add((New-Label "Domains CSV" 5)) | Out-Null
$licenseGrid.Children.Add($domainsBox) | Out-Null
$licenseGrid.Children.Add((New-Label "Machine IDs CSV" 6)) | Out-Null
$licenseGrid.Children.Add($machineBox) | Out-Null
$makeLicense = New-Button "Make License" 7
$makeLicense.Add_Click({
    $args = @("make-license", "--private-key", $licensePrivateBox.Text, "--out", $licenseOutBox.Text, "--product-id", $licenseProductBox.Text, "--customer-id", $customerBox.Text, "--expires-at", $expiresBox.Text)
    if ($domainsBox.Text) { $args += @("--domains", $domainsBox.Text) }
    if ($machineBox.Text) { $args += @("--machine-fingerprints", $machineBox.Text) }
    Invoke-PhpShield $args
})
$licenseGrid.Children.Add($makeLicense) | Out-Null

$toolsTab = New-Object Windows.Controls.TabItem
$toolsTab.Header = "Tools"
$toolsPanel = New-Object Windows.Controls.StackPanel
$toolsPanel.Margin = "8"
$toolsTab.Content = $toolsPanel
$tabs.Items.Add($toolsTab) | Out-Null

$bundleInspectBox = New-TextBox 0
$toolsPanel.Children.Add($bundleInspectBox) | Out-Null
$inspectBrowse = New-Object Windows.Controls.Button
$inspectBrowse.Content = "Browse Bundle"
$inspectBrowse.Margin = "8,4,8,4"
$inspectBrowse.Add_Click({ Select-File $bundleInspectBox "PHPShield bundle (*.pshield)|*.pshield|All files (*.*)|*.*" })
$toolsPanel.Children.Add($inspectBrowse) | Out-Null
$inspectButton = New-Object Windows.Controls.Button
$inspectButton.Content = "Inspect Bundle"
$inspectButton.Margin = "8,4,8,4"
$inspectButton.Padding = "12,4,12,4"
$inspectButton.Add_Click({ Invoke-PhpShield @("inspect", $bundleInspectBox.Text) })
$toolsPanel.Children.Add($inspectButton) | Out-Null
$initButton = New-Object Windows.Controls.Button
$initButton.Content = "Initialize Keys"
$initButton.Margin = "8,4,8,4"
$initButton.Padding = "12,4,12,4"
$initButton.Add_Click({ Invoke-PhpShield @("init") })
$toolsPanel.Children.Add($initButton) | Out-Null

$script:OutputBox = New-Object Windows.Controls.TextBox
$script:OutputBox.Margin = "10"
$script:OutputBox.Height = 210
$script:OutputBox.FontFamily = "Consolas"
$script:OutputBox.FontSize = 12
$script:OutputBox.AcceptsReturn = $true
$script:OutputBox.VerticalScrollBarVisibility = "Auto"
$script:OutputBox.HorizontalScrollBarVisibility = "Auto"
$script:OutputBox.IsReadOnly = $true
[Windows.Controls.DockPanel]::SetDock($script:OutputBox, "Bottom")
$root.Children.Add($script:OutputBox) | Out-Null

$script:OutputBox.Text = "PHPShield GUI ready. Project root: $ProjectRoot`r`n"
$window.ShowDialog() | Out-Null
