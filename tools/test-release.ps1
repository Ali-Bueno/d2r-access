# Build, test and package the complete tester ZIP (plugin, D2RLoader, VC++ runtime), then
# publish it as the latest GitHub release. See docs/test-releases.md.
#Requires -Version 7
param(
	[string]$LoaderDir = (Join-Path $PSScriptRoot "..\..\loader-backups\1.3.1-beta"),
	[string]$NewsFile,
	[switch]$NoPublish
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot
$out = Join-Path $root "build\test-release"
$buildDir = Join-Path $out "cmake"
$stage = Join-Path $out "package"
$zip = Join-Path $out "D2RAccess.zip"
$pluginDll = "d2rl-d2r-access.dll"  # OUTPUT_NAME in CMakeLists.txt
$vcRedist = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC"
$runtimeDlls = "msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll"
# No config files: the loader writes d2rloader.toml and d2r-access.toml when missing, and an
# update extracted on top must keep the player's own (docs/test-releases.md).
$loaderFiles = "D2RLoader.exe", "D2RCore.dll", "d2rloader.mpq"
$loaderDirs = "mods", "d2rloader\crashes", "d2rloader\data", "d2rloader\logs"
# Everything the plugin binary is built from; packaging text may change without a rebuild.
$buildInputs = "src", "cmake", "CMakeLists.txt", "plugin.rc", "sounds", "third_party", "radar.toml", "data", "external", "LICENSE"

function Copy-Into([string]$Source, [string]$Destination) {
	New-Item -ItemType Directory (Split-Path $Destination) -Force | Out-Null
	Copy-Item $Source $Destination
}

Push-Location $root
try {
	$commit = git rev-parse HEAD
	$short = git rev-parse --short HEAD
	$changed = git status --porcelain -- $buildInputs
	if ($changed -and -not $NoPublish) { throw "Commit the build inputs first:`n$($changed -join "`n")" }
	if ($changed) { Write-Warning "Uncommitted build inputs: the package will not match $short." }
	if (-not $NoPublish -and -not (git branch -r --contains $commit)) { throw "Push $short before publishing." }

	$LoaderDir = (Resolve-Path $LoaderDir).Path
	$loaderVersion = Split-Path $LoaderDir -Leaf
	$crt = Get-ChildItem $vcRedist -Directory | Where-Object { $_.Name -as [version] } |
		Sort-Object { [version]$_.Name } | Select-Object -Last 1 |
		ForEach-Object { Get-ChildItem (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" }
	if (-not $crt) { throw "No x64 VC++ runtime found under $vcRedist." }

	# The tag is chosen first: the plugin speaks it at start.
	$date = Get-Date -Format "yyyy-MM-dd"
	$tag = "test-$date"
	$n = 1
	while (git ls-remote --tags origin "refs/tags/$tag") { $n++; $tag = "test-$date-$n" }

	# A separate build tree whose deploy step points away from the game install.
	& (Join-Path $root "build.ps1") -BuildDir $buildDir -GameDir (Join-Path $out "deploy") -Release $tag
	if ($LASTEXITCODE) { throw "Build failed." }
	ctest --test-dir $buildDir --output-on-failure
	if ($LASTEXITCODE) { throw "Tests failed." }

	Remove-Item $stage, $zip -Recurse -Force -ErrorAction SilentlyContinue
	foreach ($file in $loaderFiles) { Copy-Into (Join-Path $LoaderDir $file) (Join-Path $stage $file) }
	foreach ($dir in $loaderDirs) { New-Item -ItemType Directory (Join-Path $stage $dir) -Force | Out-Null }
	foreach ($dll in $runtimeDlls) { Copy-Into (Join-Path $crt.FullName $dll) (Join-Path $stage $dll) }
	Copy-Into (Join-Path $buildDir "D2RAccess.exe") (Join-Path $stage "D2RAccess.exe")
	Copy-Into (Join-Path $buildDir $pluginDll) (Join-Path $stage "d2rloader\plugins\$pluginDll")
	Copy-Into "third_party\prism\bin\prism.dll" (Join-Path $stage "d2rloader\plugins\d2r-access\prism.dll")
	Copy-Into "packaging\tester\KEYBOARD.txt" (Join-Path $stage "KEYBOARD.txt")
	# The guide as a web page: in a browser a screen reader walks its tables cell by cell.
	$guide = (ConvertFrom-Markdown -Path "GUIDE.md").Html
	$guideHtml = "<!DOCTYPE html>`n<html lang=`"en`">`n<head>`n<meta charset=`"utf-8`">`n<title>Diablo II Resurrected Access: commands and tutorial</title>`n</head>`n<body>`n$guide</body>`n</html>`n"
	[IO.File]::WriteAllText((Join-Path $stage "GUIDE.html"), $guideHtml, [Text.UTF8Encoding]::new($false))
	Copy-Into "LICENSE" (Join-Path $stage "licenses\d2r-access-LICENSE.txt")
	Copy-Item "third_party\prism\LICENSES" (Join-Path $stage "licenses\prism") -Recurse
	if (Get-ChildItem $stage -Recurse -Filter *.toml) { throw "A config file is staged; an update would overwrite the player's settings." }

	$repo = gh repo view --json nameWithOwner -q .nameWithOwner
	$latestUrl = "https://github.com/$repo/releases/latest/download/$(Split-Path $zip -Leaf)"
	$news = if ($NewsFile) { (Get-Content $NewsFile -Raw -Encoding utf8).Trim() -replace "`r`n", "`n" } else { "" }
	$readmeDate = (Get-Date).ToString("MMMM d, yyyy", [Globalization.CultureInfo]::GetCultureInfo("en-US"))
	$readme = (Get-Content "packaging\tester\README.txt" -Raw -Encoding utf8) -replace "`r`n", "`n"
	$readme = $readme.Replace("{{NEWS}}`n", $(if ($news) { "WHAT'S NEW`n`n$news`n`n`n" } else { "" }))
	$readme = $readme.Replace("{{DATE}}", $readmeDate).Replace("{{COMMIT}}", $short)
	$readme = $readme.Replace("{{LOADER}}", $loaderVersion).Replace("{{LATEST_URL}}", $latestUrl)
	# Notepad-friendly: CRLF with a BOM.
	[IO.File]::WriteAllText((Join-Path $stage "README.txt"), ($readme -replace "`n", "`r`n"), [Text.UTF8Encoding]::new($true))

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
	Write-Host "$zip`n$entryCount entries, $((Get-Item $zip).Length) bytes, SHA-256 $zipHash"
	if ($NoPublish) { return }

	$notes = @(
		"Test build with the complete package: the mod (commit $short), D2RLoader $loaderVersion and the Visual C++ runtime."
		if ($news) { "", "## What's new", "", $news }
		"", "## Install or update", ""
		"Close the game, download ``$(Split-Path $zip -Leaf)`` and extract everything into the game folder (the one with ``D2R.exe``). The full instructions are in ``README.txt``, inside the zip."
		"", "Direct link that always downloads the latest version:", "", $latestUrl
		"", "Zip SHA-256: ``$zipHash``"
	) -join "`n"
	$releaseNotes = Join-Path $out "notes.md"
	[IO.File]::WriteAllText($releaseNotes, $notes, [Text.UTF8Encoding]::new($false))
	# The launcher reads this manifest to offer the update (docs/launcher.md, "Self-update").
	$releaseJson = Join-Path $out "release.json"
	$manifest = [ordered]@{ tag = $tag; asset = (Split-Path $zip -Leaf); sha256 = $zipHash; notes = $notes } | ConvertTo-Json
	[IO.File]::WriteAllText($releaseJson, $manifest, [Text.UTF8Encoding]::new($false))
	gh release create $tag $zip $releaseJson --target $commit --title "Test build $($tag -replace '^test-', '')" --notes-file $releaseNotes --latest
	if ($LASTEXITCODE) { throw "Publishing failed." }
	Write-Host "Published $tag. Always-latest link: $latestUrl"
} finally {
	Pop-Location
}
