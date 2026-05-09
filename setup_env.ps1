# Zenith OS Environment Setup Script
Write-Host "Setting up Python virtual environment..." -ForegroundColor Cyan

if (!(Test-Path "venv")) {
    python -m venv venv
    Write-Host "Virtual environment created." -ForegroundColor Green
}

Write-Host "Activating environment and installing dependencies..." -ForegroundColor Cyan
& .\venv\Scripts\Activate.ps1
pip install -r requirements.txt

Write-Host "Environment ready! Remember to run '.\venv\Scripts\Activate.ps1' in new terminals." -ForegroundColor Green
