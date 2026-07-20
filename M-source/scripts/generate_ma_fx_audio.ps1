$ErrorActionPreference = "Stop"

$SampleRate = 48000
$TwoPi = 2.0 * [Math]::PI

$SourceRoot = Split-Path -Parent $PSScriptRoot
$WorkspaceRoot = Split-Path -Parent $SourceRoot
$SharedClientRoot = Get-ChildItem -LiteralPath $WorkspaceRoot -Directory | Where-Object { $_.Name -like "M*client" } | Select-Object -First 1
$SourceAudioRoot = Join-Path $SourceRoot "data/audio"
$TargetAudioRoots = @(
	(Join-Path $WorkspaceRoot "TClient-10.8.7-win64/data/audio"),
	(Join-Path $WorkspaceRoot "cliente ma/data/audio")
)
if($SharedClientRoot) {
	$TargetAudioRoots += Join-Path $SharedClientRoot.FullName "data/audio"
}
$Packs = @(
	[pscustomobject]@{ Id = "ma_space_pulse"; Style = "space_pulse" },
	[pscustomobject]@{ Id = "ma_retro_arcade"; Style = "retro_arcade" },
	[pscustomobject]@{ Id = "ma_demon_core"; Style = "demon_core" },
	[pscustomobject]@{ Id = "ma_magic_stars"; Style = "magic_stars" },
	[pscustomobject]@{ Id = "ma_dark_void"; Style = "dark_void" }
)

function Get-StableSeed {
	param([string]$Text, [int]$Variant)
	$Seed = 2166136261
	foreach($Char in $Text.ToCharArray()) {
		$Seed = ($Seed -bxor [int][char]$Char) * 16777619
		$Seed = $Seed -band 0x7fffffff
	}
	return ($Seed + $Variant * 977) -band 0x7fffffff
}

function Get-Duration {
	param([string]$Kind)
	switch -Wildcard ($Kind) {
		"hook_loop" { return 0.20 }
		"hook_attach" { return 0.12 }
		"hook_noattach" { return 0.10 }
		"gun" { return 0.09 }
		"shotgun" { return 0.15 }
		"grenade_launch" { return 0.18 }
		"grenade_explode" { return 0.26 }
		"hammer_swing" { return 0.13 }
		"hammer_hit" { return 0.14 }
		"laser_fire" { return 0.12 }
		"laser_bounce" { return 0.10 }
		"switch" { return 0.075 }
		"foot" { return 0.07 }
		"land" { return 0.11 }
		"dbljump" { return 0.13 }
		"skid" { return 0.08 }
		"body" { return 0.15 }
		"pickup" { return 0.12 }
		"hit" { return 0.10 }
		"ctf" { return 0.16 }
		"spawn" { return 0.15 }
		default { return 0.12 }
	}
}

function New-MaFxSamples {
	param([string]$Kind, [int]$Variant, [string]$Style)

	$Duration = Get-Duration $Kind
	$Count = [Math]::Max(1, [int]($Duration * $SampleRate))
	$Samples = New-Object double[] $Count
	$Random = [System.Random]::new((Get-StableSeed "$Style-$Kind" $Variant))

	for($Index = 0; $Index -lt $Count; $Index++) {
		$T = $Index / [double]$SampleRate
		$U = $Index / [double]$Count
		$Env = [Math]::Pow([Math]::Max(0.0, 1.0 - $U), 2.35)
		$Noise = $Random.NextDouble() * 2.0 - 1.0
		$Signal = 0.0

		switch -Wildcard ($Kind) {
			"hook_attach" {
				$Signal = 0.38 * [Math]::Sin($TwoPi * (850.0 + $Variant * 120.0 + 900.0 * $U) * $T) * [Math]::Exp(-20.0 * $T)
				$Signal += 0.22 * [Math]::Sin($TwoPi * (2200.0 + $Variant * 140.0) * $T) * [Math]::Exp(-38.0 * $T)
				$Signal += 0.14 * $Noise * [Math]::Exp(-58.0 * $T)
			}
			"hook_loop" {
				$Base = 180.0 + (($Variant - 1) * 40.0)
				$Fade = [Math]::Sin([Math]::PI * $U)
				$Signal = (0.13 * [Math]::Sin($TwoPi * $Base * $T) + 0.04 * [Math]::Sin($TwoPi * ($Base * 2.0) * $T)) * $Fade
			}
			"hook_noattach" {
				$Freq = 1400.0 + $Variant * 90.0 - 650.0 * $U
				$Signal = (0.34 * [Math]::Sin($TwoPi * $Freq * $T) + 0.10 * $Noise) * [Math]::Exp(-28.0 * $T)
			}
			"gun" {
				$Freq = 2100.0 + $Variant * 180.0 - 1200.0 * $U
				$Signal = (0.46 * [Math]::Sin($TwoPi * $Freq * $T) + 0.20 * $Noise) * [Math]::Exp(-34.0 * $T)
			}
			"shotgun" {
				$Signal = (0.36 * $Noise + 0.24 * [Math]::Sin($TwoPi * (140.0 + $Variant * 18.0) * $T)) * [Math]::Exp(-18.0 * $T)
			}
			"grenade_launch" {
				$Freq = 130.0 + 210.0 * $U + $Variant * 12.0
				$Signal = (0.34 * [Math]::Sin($TwoPi * $Freq * $T) + 0.12 * $Noise) * [Math]::Exp(-13.0 * $T)
			}
			"grenade_explode" {
				$Signal = (0.44 * $Noise + 0.30 * [Math]::Sin($TwoPi * (70.0 + $Variant * 10.0) * $T)) * [Math]::Exp(-9.0 * $T)
			}
			"hammer_swing" {
				$Freq = 420.0 + 620.0 * $U
				$Sweep = [Math]::Sin([Math]::PI * $U)
				$Signal = (0.18 * [Math]::Sin($TwoPi * $Freq * $T) + 0.22 * $Noise) * $Sweep * [Math]::Exp(-4.0 * $T)
			}
			"hammer_hit" {
				$Signal = (0.42 * [Math]::Sin($TwoPi * (95.0 + $Variant * 12.0) * $T) + 0.18 * $Noise) * [Math]::Exp(-20.0 * $T)
			}
			"laser_fire" {
				$Freq = 2600.0 + 1400.0 * $U + $Variant * 120.0
				$Signal = (0.38 * [Math]::Sin($TwoPi * $Freq * $T) + 0.12 * [Math]::Sin($TwoPi * ($Freq * 0.5) * $T)) * [Math]::Exp(-18.0 * $T)
			}
			"laser_bounce" {
				$Signal = 0.36 * [Math]::Sin($TwoPi * (1750.0 + $Variant * 160.0) * $T) * [Math]::Exp(-28.0 * $T)
			}
			"switch" {
				$Signal = 0.42 * [Math]::Sin($TwoPi * (1200.0 + $Variant * 130.0) * $T) * [Math]::Exp(-80.0 * $T)
				if($T -gt 0.032) {
					$Signal += 0.22 * [Math]::Sin($TwoPi * (1900.0 + $Variant * 80.0) * ($T - 0.032)) * [Math]::Exp(-95.0 * ($T - 0.032))
				}
			}
			"foot" {
				$Signal = (0.22 * $Noise + 0.14 * [Math]::Sin($TwoPi * (180.0 + $Variant * 18.0) * $T)) * [Math]::Exp(-45.0 * $T)
			}
			"land" {
				$Signal = (0.30 * [Math]::Sin($TwoPi * (105.0 + $Variant * 10.0) * $T) + 0.18 * $Noise) * [Math]::Exp(-24.0 * $T)
			}
			"dbljump" {
				$Freq = 560.0 + 720.0 * $U + $Variant * 45.0
				$Signal = (0.22 * [Math]::Sin($TwoPi * $Freq * $T) + 0.12 * $Noise) * [Math]::Exp(-16.0 * $T)
			}
			"skid" {
				$Signal = 0.22 * $Noise * [Math]::Sin([Math]::PI * $U) * [Math]::Exp(-7.0 * $T)
			}
			"body" {
				$Signal = (0.32 * [Math]::Sin($TwoPi * (90.0 + $Variant * 8.0) * $T) + 0.20 * $Noise) * [Math]::Exp(-15.0 * $T)
			}
			"pickup" {
				$Freq = 760.0 + 1500.0 * $U + $Variant * 80.0
				$Signal = (0.28 * [Math]::Sin($TwoPi * $Freq * $T) + 0.10 * [Math]::Sin($TwoPi * ($Freq * 2.0) * $T)) * [Math]::Exp(-10.0 * $T)
			}
			"hit" {
				$Signal = (0.30 * [Math]::Sin($TwoPi * (620.0 + $Variant * 90.0) * $T) + 0.18 * $Noise) * [Math]::Exp(-24.0 * $T)
			}
			"ctf" {
				$Freq = 520.0 + 1800.0 * $U
				$Signal = (0.25 * [Math]::Sin($TwoPi * $Freq * $T) + 0.12 * [Math]::Sin($TwoPi * ($Freq * 1.5) * $T)) * [Math]::Exp(-7.0 * $T)
			}
			"spawn" {
				$Freq = 420.0 + 900.0 * $U + $Variant * 40.0
				$Signal = (0.28 * [Math]::Sin($TwoPi * $Freq * $T) + 0.08 * $Noise) * [Math]::Exp(-10.0 * $T)
			}
		}

		switch($Style) {
			"retro_arcade" {
				$ToneBase = 220.0 + (($Variant % 5) * 55.0)
				$Step = [Math]::Floor($U * 8.0)
				$Tone = $ToneBase * [Math]::Pow(2.0, (($Step % 7.0) / 12.0))
				$Square = -1.0
				if([Math]::Sin($TwoPi * $Tone * $T) -ge 0.0) {
					$Square = 1.0
				}
				$Pulse = -0.45
				if([Math]::Sin($TwoPi * ($Tone * 2.0) * $T) -ge 0.0) {
					$Pulse = 0.45
				}
				$Signal = ($Signal * 0.22 + $Square * 0.23 + $Pulse * 0.10) * $Env
				$Signal = [Math]::Round($Signal * 12.0) / 12.0
			}
			"demon_core" {
				$Low = [Math]::Sin($TwoPi * (48.0 + $Variant * 7.0 + 80.0 * $U) * $T)
				$Growl = [Math]::Sin($TwoPi * (92.0 + $Variant * 11.0) * $T)
				$Signal = [Math]::Tanh(($Signal * 2.7 + $Low * 0.45 + $Growl * 0.18 + $Noise * 0.20 * [Math]::Exp(-7.0 * $T)) * 1.7) * 0.70
			}
			"magic_stars" {
				$SparkleA = [Math]::Sin($TwoPi * (1680.0 + $Variant * 150.0 + 2600.0 * $U) * $T) * [Math]::Exp(-11.0 * $T)
				$SparkleB = [Math]::Sin($TwoPi * (2940.0 + $Variant * 210.0 + 3600.0 * $U) * $T) * [Math]::Exp(-18.0 * $T)
				$Twinkle = 0.0
				if([Math]::Sin($TwoPi * (11.0 + $Variant) * $T) -gt 0.55) {
					$Twinkle = 1.0
				}
				$Signal = $Signal * 0.32 + $SparkleA * 0.28 + $SparkleB * 0.16 + $Noise * 0.035 * $Twinkle
			}
			"dark_void" {
				$Low = [Math]::Sin($TwoPi * (36.0 + $Variant * 5.0 + 45.0 * $U) * $T)
				$Wobble = 0.72 + 0.28 * [Math]::Sin($TwoPi * (3.0 + $Variant * 0.3) * $T)
				$Signal = ($Signal * 0.16 + $Low * 0.48 + $Noise * 0.09) * $Wobble * [Math]::Exp(-2.6 * $T)
			}
			default {
				$Signal = $Signal * 1.0
			}
		}

		if($Index -lt 96) {
			$Signal *= $Index / 96.0
		}
		if($Count - $Index -lt 96) {
			$Signal *= ($Count - $Index) / 96.0
		}
		$Samples[$Index] = [Math]::Max(-0.95, [Math]::Min(0.95, $Signal))
	}

	return $Samples
}

function Write-Wav {
	param([string]$Path, [double[]]$Samples)

	$Directory = Split-Path -Parent $Path
	New-Item -ItemType Directory -Force -Path $Directory | Out-Null

	$DataSize = $Samples.Length * 2
	$Stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
	try {
		$Writer = [System.IO.BinaryWriter]::new($Stream)
		$Writer.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
		$Writer.Write([UInt32](36 + $DataSize))
		$Writer.Write([System.Text.Encoding]::ASCII.GetBytes("WAVE"))
		$Writer.Write([System.Text.Encoding]::ASCII.GetBytes("fmt "))
		$Writer.Write([UInt32]16)
		$Writer.Write([UInt16]1)
		$Writer.Write([UInt16]1)
		$Writer.Write([UInt32]$SampleRate)
		$Writer.Write([UInt32]($SampleRate * 2))
		$Writer.Write([UInt16]2)
		$Writer.Write([UInt16]16)
		$Writer.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
		$Writer.Write([UInt32]$DataSize)
		foreach($Sample in $Samples) {
			$Value = [Math]::Max(-1.0, [Math]::Min(1.0, $Sample))
			$Writer.Write([Int16][Math]::Round($Value * 32767.0))
		}
	}
	finally {
		if($Writer) {
			$Writer.Dispose()
		}
		else {
			$Stream.Dispose()
		}
	}
}

$Files = New-Object System.Collections.Generic.List[object]
function Add-File {
	param([string]$Name, [string]$Kind, [int]$Variant = 1)
	$Files.Add([pscustomobject]@{ Name = $Name; Kind = $Kind; Variant = $Variant }) | Out-Null
}
function Add-Series {
	param([string]$Prefix, [int]$Count, [string]$Kind)
	for($Index = 1; $Index -le $Count; $Index++) {
		Add-File ("{0}-{1:D2}.wav" -f $Prefix, $Index) $Kind $Index
	}
}

Add-Series "hook_attach" 3 "hook_attach"
Add-Series "hook_loop" 2 "hook_loop"
Add-Series "hook_noattach" 3 "hook_noattach"
Add-Series "wp_gun_fire" 3 "gun"
Add-Series "wp_shotty_fire" 3 "shotgun"
Add-Series "wp_flump_launch" 3 "grenade_launch"
Add-Series "wp_flump_explo" 3 "grenade_explode"
Add-Series "wp_hammer_swing" 3 "hammer_swing"
Add-Series "wp_hammer_hit" 3 "hammer_hit"
Add-Series "wp_laser_fire" 3 "laser_fire"
Add-Series "wp_laser_bnce" 3 "laser_bounce"
Add-Series "wp_switch" 3 "switch"
Add-Series "foley_foot_left" 4 "foot"
Add-Series "foley_foot_right" 4 "foot"
Add-Series "foley_land" 4 "land"
Add-Series "foley_dbljump" 3 "dbljump"
Add-Series "foley_body_impact" 3 "body"
Add-Series "foley_body_splat" 4 "body"
Add-Series "sfx_skid" 4 "skid"
Add-Series "sfx_hit_strong" 2 "hit"
Add-Series "sfx_hit_weak" 3 "hit"
Add-Series "sfx_spawn_wpn" 3 "spawn"
Add-File "sfx_pickup_gun.wav" "pickup" 1
Add-File "sfx_pickup_sg.wav" "pickup" 2
Add-File "sfx_pickup_launcher.wav" "pickup" 3
Add-File "sfx_pickup_ninja.wav" "pickup" 4
Add-Series "sfx_pickup_arm" 4 "pickup"
Add-Series "sfx_pickup_hrt" 2 "pickup"
Add-File "sfx_ctf_cap_pl.wav" "ctf" 1
Add-File "sfx_ctf_drop.wav" "ctf" 2
Add-File "sfx_ctf_grab_en.wav" "ctf" 3
Add-File "sfx_ctf_grab_pl.wav" "ctf" 4
Add-File "sfx_ctf_rtn.wav" "ctf" 5

foreach($Pack in $Packs) {
	$SourcePackRoot = Join-Path $SourceAudioRoot $Pack.Id
	New-Item -ItemType Directory -Force -Path $SourcePackRoot | Out-Null
	foreach($File in $Files) {
		$Samples = New-MaFxSamples $File.Kind $File.Variant $Pack.Style
		Write-Wav (Join-Path $SourcePackRoot $File.Name) $Samples
	}

	foreach($TargetAudioRoot in $TargetAudioRoots) {
		New-Item -ItemType Directory -Force -Path $TargetAudioRoot | Out-Null
		Copy-Item -LiteralPath $SourcePackRoot -Destination $TargetAudioRoot -Recurse -Force
	}
}

Write-Host ("Generated {0} files for {1} packs." -f $Files.Count, $Packs.Count)
