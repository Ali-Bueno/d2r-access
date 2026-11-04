# Build, test and package the complete tester ZIP (plugin, D2RLoader, VC++ runtime) and the
# installer, then publish both as the latest GitHub release. See docs/test-releases.md.
#Requires -Version 7
param(
	[string]$LoaderDir = (Join-Path $PSScriptRoot "..\..\loader-backups\1.3.1-beta"),
	[string]$InstallerDir = (Join-Path $PSScriptRoot "..\..\..\..\tools\mod-installer"),
	[string]$NewsFile,
	# A numbered release (1.0, 1.0.1, 1.1): its tag, which the plugin speaks at start and the launcher's
	# update check reads as newer than every dated build. Without it, a dated test build (test-YYYY-MM-DD[-N]).
	[string]$Version,
	[switch]$NoPublish
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot
$out = Join-Path $root "build\test-release"
$buildDir = Join-Path $out "cmake"
$stage = Join-Path $out "package"
$zip = Join-Path $out "D2RAccess.zip"
$updateZip = Join-Path $out "D2RUpdate.zip" # a name the installer's "D2RAccess*.zip" does not match
$shippedPrism = "CB9712E11AF9EBE96457DBF8F5DAAD4A6C359AE1F59CDF2663282B3A9CC9759C" # every release since 1.0
$installer = Join-Path $out "D2RAccessInstaller.exe"
$pluginDll = "d2rl-d2r-access.dll"  # OUTPUT_NAME in CMakeLists.txt
$vcRedist = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC"
$runtimeDlls = "msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll"
# No config files: the loader writes d2rloader.toml and d2r-access.toml when missing, and an
# update extracted on top must keep the player's own (docs/test-releases.md).
$loaderFiles = "D2RLoader.exe", "D2RCore.dll", "d2rloader.mpq"
$loaderDirs = "mods", "d2rloader\crashes", "d2rloader\data", "d2rloader\logs"
# Everything the plugin binary is built from; packaging text may change without a rebuild.
$buildInputs = "src", "launcher", "cmake", "CMakeLists.txt", "plugin.rc", "sounds", "third_party", "radar.toml", "data", "external", "LICENSE"

function Copy-Into([string]$Source, [string]$Destination) {
	New-Item -ItemType Directory (Split-Path $Destination) -Force | Out-Null
	Copy-Item $Source $Destination
}

# The player documentation ships as web pages: in a browser a screen reader walks the headings,
# and the tables cell by cell.
function Write-Page([string]$Markdown, [string]$Title, [string]$Destination) {
	$style = ":root{color-scheme:light dark}body{font-family:system-ui,sans-serif;line-height:1.5;max-width:60em;margin:auto;padding:1em}" +
		"table{border-collapse:collapse}th,td{border:1px solid #888;padding:.3em .6em;text-align:left;vertical-align:top}"
	$body = (ConvertFrom-Markdown -InputObject $Markdown).Html
	$html = "<!DOCTYPE html>`n<html lang=`"en`">`n<head>`n<meta charset=`"utf-8`">`n<title>$Title</title>`n<style>$style</style>`n</head>`n<body>`n$body</body>`n</html>`n"
	[IO.File]::WriteAllText($Destination, $html, [Text.UTF8Encoding]::new($false))
}

Push-Location $root
try {
	$commit = git rev-parse HEAD
	$short = git rev-parse --short HEAD
	$changed = git status --porcelain -- $buildInputs
	if ($changed -and -not $NoPublish) { throw "Commit the build inputs first:`n$($changed -join "`n")" }
	if ($changed) { Write-Warning "Uncommitted build inputs: the package will not match $short." }
	if (-not $NoPublish -and -not (git branch -r --contains $commit)) { throw "Push $short before publishing." }
	$InstallerDir = (Resolve-Path $InstallerDir).Path
	$installerChanged = git -C $InstallerDir status --porcelain
	if ($installerChanged -and -not $NoPublish) { throw "Commit the installer first ($InstallerDir):`n$($installerChanged -join "`n")" }

	$LoaderDir = (Resolve-Path $LoaderDir).Path
	$loaderVersion = Split-Path $LoaderDir -Leaf
	$crt = Get-ChildItem $vcRedist -Directory | Where-Object { $_.Name -as [version] } |
		Sort-Object { [version]$_.Name } | Select-Object -Last 1 |
		ForEach-Object { Get-ChildItem (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" }
	if (-not $crt) { throw "No x64 VC++ runtime found under $vcRedist." }

	# The tag is chosen first: the plugin speaks it at start. A failed query is no answer (GitHub said
	# "repository is disabled" for a few seconds after the repo turned public).
	function Test-RemoteTag([string]$Name) {
		$found = git ls-remote --tags origin "refs/tags/$Name"
		if ($LASTEXITCODE) { throw "Could not read GitHub's tags (git ls-remote failed)." }
		[bool]$found
	}
	if ($Version) {
		if ($Version -notmatch '^[1-9]\d{0,3}\.(0|[1-9]\d{0,3})(\.(0|[1-9]\d{0,3}))?$') { throw "-Version takes MAJOR.MINOR[.PATCH], like 1.0." }
		$tag = $Version
		if (Test-RemoteTag $tag) { throw "The tag $tag is already on GitHub." }
	} else {
		$date = Get-Date -Format "yyyy-MM-dd"
		$tag = "test-$date"
		$n = 1
		while (Test-RemoteTag $tag) { $n++; $tag = "test-$date-$n" }
	}
	$title = if ($Version) { "D2R Access $Version" } else { "Test build $($tag -replace '^test-', '')" }

	# A separate build tree whose deploy step points away from the game install.
	& (Join-Path $root "build.ps1") -BuildDir $buildDir -GameDir (Join-Path $out "deploy") -Release $tag
	if ($LASTEXITCODE) { throw "Build failed." }
	ctest --test-dir $buildDir --output-on-failure
	if ($LASTEXITCODE) { throw "Tests failed." }
	# The installer (docs/release-launch.md) comes from its own repo, built with this mod's config.
	$env:MOD_CONFIG = "d2r"
	try { cargo build --release --manifest-path (Join-Path $InstallerDir "Cargo.toml") } finally { Remove-Item Env:MOD_CONFIG }
	if ($LASTEXITCODE) { throw "Installer build failed." }

	Remove-Item $stage, $zip, $installer -Recurse -Force -ErrorAction SilentlyContinue
	Copy-Item (Join-Path $InstallerDir "target\release\installer.exe") $installer
	foreach ($file in $loaderFiles) { Copy-Into (Join-Path $LoaderDir $file) (Join-Path $stage $file) }
	foreach ($dir in $loaderDirs) { New-Item -ItemType Directory (Join-Path $stage $dir) -Force | Out-Null }
	foreach ($dll in $runtimeDlls) { Copy-Into (Join-Path $crt.FullName $dll) (Join-Path $stage $dll) }
	Copy-Into (Join-Path $buildDir "D2RAccess.exe") (Join-Path $stage "D2RAccess.exe")
	Copy-Into (Join-Path $buildDir $pluginDll) (Join-Path $stage "d2rloader\plugins\$pluginDll")
	Copy-Into "third_party\prism\bin\prism.dll" (Join-Path $stage "d2rloader\plugins\d2r-access\prism.dll")
	Write-Page (Get-Content "GUIDE.md" -Raw -Encoding utf8) "Diablo II: Resurrected Access: commands and tutorial" (Join-Path $stage "GUIDE.html")
	Copy-Into "LICENSE" (Join-Path $stage "licenses\d2r-access-LICENSE.txt")
	Copy-Item "third_party\prism\LICENSES" (Join-Path $stage "licenses\prism") -Recurse
	if (Get-ChildItem $stage -Recurse -Filter *.toml) { throw "A config file is staged; an update would overwrite the player's settings." }

	$repo = gh repo view --json nameWithOwner -q .nameWithOwner
	$latestUrl = "https://github.com/$repo/releases/latest/download/$(Split-Path $zip -Leaf)"
	$installerUrl = "https://github.com/$repo/releases/latest/download/$(Split-Path $installer -Leaf)"
	$news = if ($NewsFile) { (Get-Content $NewsFile -Raw -Encoding utf8).Trim() -replace "`r`n", "`n" } else { "" }
	$readmeDate = (Get-Date).ToString("MMMM d, yyyy", [Globalization.CultureInfo]::GetCultureInfo("en-US"))
	$readme = (Get-Content "packaging\README.md" -Raw -Encoding utf8) -replace "`r`n", "`n"
	$readme = $readme.Replace("{{NEWS}}`n", $(if ($news) { "## What's new`n`n$news`n" } else { "" }))
	$readme = $readme.Replace("{{VERSION}}", $(if ($Version) { "$Version, $readmeDate" } else { "of $readmeDate" })).Replace("{{COMMIT}}", $short)
	$readme = $readme.Replace("{{LOADER}}", $loaderVersion).Replace("{{LATEST_URL}}", $latestUrl).Replace("{{INSTALLER_URL}}", $installerUrl)
	if ($readme.Contains("{{")) { throw "packaging\README.md has a placeholder the script does not fill." }
	Write-Page $readme "Diablo II: Resurrected Access" (Join-Path $stage "README.html")

	Add-Type -AssemblyName System.IO.Compression.FileSystem
	[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
	$archive = [IO.Compression.ZipFile]::OpenRead($zip)
	try {
		foreach ($entry in $archive.Entries | Where-Object Name) {
			$stream = $entry.Open()
			try { $hash = (Get-FileHash -InputStream $stream).Hash } finally { $stream.Dispose() }
			if ($hash -ne (Get-FileHash (Join-Path $stage $entry.FullName)).Hash) { throw "Zip entry differs from its source: $($entry.FullName)" }
		}
		$entryCount = $archive.Entries.Count
	} finally { $archive.Dispose() }
	$zipHash = (Get-FileHash $zip).Hash.ToLower()
	# Launchers 1.0.1 to 1.0.4 keep prism.dll open and give up on overwriting it, so the zip they update from
	# leaves it out (docs/launcher.md, "Self-update"). A new PRISM cannot reach those players this way.
	$prismEntry = "d2rloader/plugins/d2r-access/prism.dll"
	if ((Get-FileHash (Join-Path $stage $prismEntry)).Hash -ne $shippedPrism) { throw "prism.dll changed: decide how it reaches launchers before 1.0.5." }
	Copy-Item $zip $updateZip -Force
	$archive = [IO.Compression.ZipFile]::Open($updateZip, [IO.Compression.ZipArchiveMode]::Update)
	try { $archive.GetEntry($prismEntry).Delete() } finally { $archive.Dispose() }
	$updateHash = (Get-FileHash $updateZip).Hash.ToLower()
	Write-Host "$zip`n$entryCount entries, $((Get-Item $zip).Length) bytes, SHA-256 $zipHash`n$updateZip`n$((Get-Item $updateZip).Length) bytes, SHA-256 $updateHash`n$installer"
	if ($NoPublish) { return }

	$notes = @(
		"$(if ($Version) { "D2R Access $Version" } else { "Test build" }) with the complete package: the mod (commit $short), D2RLoader $loaderVersion and the Visual C++ runtime."
		if ($news) { "", "## What's new", "", $news }
		"", "## Install or update", ""
		"Close the game, download ``$(Split-Path $zip -Leaf)`` and extract everything into the game folder (the one with ``D2R.exe``). The full instructions are in ``README.html``, inside the zip."
		"", "Direct link that always downloads the latest version:", "", $latestUrl
		"", "Or run ``$(Split-Path $installer -Leaf)``: it finds the game, downloads the latest zip and installs it, keeping your settings. It is always at:", "", $installerUrl
		"", "Zip SHA-256: ``$zipHash``"
	) -join "`n"
	$releaseNotes = Join-Path $out "notes.md"
	[IO.File]::WriteAllText($releaseNotes, $notes, [Text.UTF8Encoding]::new($false))
	# The launcher reads this manifest to offer the update (docs/launcher.md, "Self-update"). Its notes are
	# only the news: the release notes also tell web readers how to download the zip.
	$releaseJson = Join-Path $out "release.json"
	$manifest = [ordered]@{ tag = $tag; asset = (Split-Path $updateZip -Leaf); sha256 = $updateHash; notes = $news } | ConvertTo-Json
	[IO.File]::WriteAllText($releaseJson, $manifest, [Text.UTF8Encoding]::new($false))
	# The pages on their own too: the installer's guide and README buttons open them before the mod is installed.
	gh release create $tag $zip $updateZip $installer $releaseJson (Join-Path $stage "README.html") (Join-Path $stage "GUIDE.html") --target $commit --title $title --notes-file $releaseNotes --latest
	if ($LASTEXITCODE) { throw "Publishing failed." }
	Write-Host "Published $tag. Always-latest link: $latestUrl"
} finally {
	Pop-Location
}
