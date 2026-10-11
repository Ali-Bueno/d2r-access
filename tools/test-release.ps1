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
$zip = Join-Path $out "d2r-access-completo.zip"
$pluginDll = "d2rl-d2r-access.dll"  # OUTPUT_NAME in CMakeLists.txt
$vcRedist = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC"
$runtimeDlls = "msvcp140.dll", "msvcp140_atomic_wait.dll", "vcruntime140.dll", "vcruntime140_1.dll"
$loaderFiles = "D2RLoader.exe", "D2RCore.dll", "d2rloader.mpq", "d2rloader\config\d2rloader.toml"
$loaderDirs = "mods", "d2rloader\crashes", "d2rloader\data", "d2rloader\logs"
# Everything the plugin binary is built from; packaging text may change without a rebuild.
$buildInputs = "src", "cmake", "CMakeLists.txt", "plugin.rc", "sounds", "third_party", "radar.toml", "external", "LICENSE"

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

	# A separate build tree whose deploy step points away from the game install.
	& (Join-Path $root "build.ps1") -BuildDir $buildDir -GameDir (Join-Path $out "deploy")
	if ($LASTEXITCODE) { throw "Build failed." }
	ctest --test-dir $buildDir --output-on-failure
	if ($LASTEXITCODE) { throw "Tests failed." }

	Remove-Item $stage, $zip -Recurse -Force -ErrorAction SilentlyContinue
	foreach ($file in $loaderFiles) { Copy-Into (Join-Path $LoaderDir $file) (Join-Path $stage $file) }
	foreach ($dir in $loaderDirs) { New-Item -ItemType Directory (Join-Path $stage $dir) -Force | Out-Null }
	foreach ($dll in $runtimeDlls) { Copy-Into (Join-Path $crt.FullName $dll) (Join-Path $stage $dll) }
	Copy-Into (Join-Path $buildDir $pluginDll) (Join-Path $stage "d2rloader\plugins\$pluginDll")
	Copy-Into "third_party\prism\bin\prism.dll" (Join-Path $stage "d2rloader\plugins\d2r-access\prism.dll")
	Copy-Into "radar.toml" (Join-Path $stage "d2rloader\config\d2r-access.toml")
	Copy-Into "packaging\tester\COMANDOS-TECLADO.txt" (Join-Path $stage "COMANDOS-TECLADO.txt")
	Copy-Into "LICENSE" (Join-Path $stage "licencias\d2r-access-LICENSE.txt")
	Copy-Item "third_party\prism\LICENSES" (Join-Path $stage "licencias\prism") -Recurse

	$repo = gh repo view --json nameWithOwner -q .nameWithOwner
	$latestUrl = "https://github.com/$repo/releases/latest/download/$(Split-Path $zip -Leaf)"
	$news = if ($NewsFile) { (Get-Content $NewsFile -Raw -Encoding utf8).Trim() -replace "`r`n", "`n" } else { "" }
	$spanishDate = (Get-Date).ToString("d 'de' MMMM 'de' yyyy", [Globalization.CultureInfo]::GetCultureInfo("es-ES"))
	$readme = (Get-Content "packaging\tester\LEEME.txt" -Raw -Encoding utf8) -replace "`r`n", "`n"
	$readme = $readme.Replace("{{NEWS}}`n", $(if ($news) { "NOVEDADES`n`n$news`n`n`n" } else { "" }))
	$readme = $readme.Replace("{{DATE}}", $spanishDate).Replace("{{COMMIT}}", $short)
	$readme = $readme.Replace("{{LOADER}}", $loaderVersion).Replace("{{LATEST_URL}}", $latestUrl)
	# Notepad-friendly: CRLF with a BOM.
	[IO.File]::WriteAllText((Join-Path $stage "LEEME.txt"), ($readme -replace "`n", "`r`n"), [Text.UTF8Encoding]::new($true))

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

	$date = Get-Date -Format "yyyy-MM-dd"
	$tag = "test-$date"
	$n = 1
	while (git ls-remote --tags origin "refs/tags/$tag") { $n++; $tag = "test-$date-$n" }
	$notes = @(
		"Versión de prueba con el paquete completo: el mod (commit $short), D2RLoader $loaderVersion y el runtime de Visual C++."
		if ($news) { "", "## Novedades", "", $news }
		"", "## Instalar o actualizar", ""
		"Cierra el juego, descarga ``$(Split-Path $zip -Leaf)`` y extrae todo dentro de la carpeta del juego (la que tiene ``D2R.exe``). Las instrucciones completas están en ``LEEME.txt``, dentro del zip."
		"", "Enlace directo que siempre baja la última versión:", "", $latestUrl
		"", "SHA-256 del zip: ``$zipHash``"
	) -join "`n"
	$releaseNotes = Join-Path $out "notes.md"
	[IO.File]::WriteAllText($releaseNotes, $notes, [Text.UTF8Encoding]::new($false))
	gh release create $tag $zip --target $commit --title "Versión de prueba $($tag -replace '^test-', '')" --notes-file $releaseNotes --latest
	if ($LASTEXITCODE) { throw "Publishing failed." }
	Write-Host "Published $tag. Always-latest link: $latestUrl"
} finally {
	Pop-Location
}
