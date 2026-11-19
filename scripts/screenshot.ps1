param(
    [string]$OutputPath = "$PSScriptRoot/../ui_test.png"
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Start-Sleep -Milliseconds 500
$p = Get-Process -Name ToneForge -ErrorAction SilentlyContinue
if ($p -and $p.MainWindowHandle -ne [IntPtr]::Zero) {
    Add-Type @'
    using System;
    using System.Runtime.InteropServices;
    public class WinUtil {
        [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
        [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    }
'@
    $rect = New-Object WinUtil+RECT
    [WinUtil]::GetWindowRect($p.MainWindowHandle, [ref]$rect)
    $w = $rect.Right - $rect.Left
    $h = $rect.Bottom - $rect.Top
    if ($w -gt 50 -and $h -gt 50) {
        $bmp = New-Object System.Drawing.Bitmap($w, $h)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
        $targetFile = [System.IO.Path]::GetFullPath($OutputPath)
        $bmp.Save($targetFile)
        $g.Dispose()
        $bmp.Dispose()
        Write-Host "Screenshot saved to: $targetFile (${w}x${h})"
    }
}

