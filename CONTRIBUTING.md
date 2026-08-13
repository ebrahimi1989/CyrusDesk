# Contributing to CyrusDesk

Thanks for taking the time to contribute! 🎉

## Getting Started

1. Fork the repository.
2. Clone your fork:
   ```bash
   git clone https://github.com/<your-username>/CyrusDesk.git
   cd CyrusDesk
   ```
3. Install dependencies (Ubuntu/Debian):
   ```bash
   sudo apt-get install -y qt5-qmake libavcodec-dev libavutil-dev libswscale-dev libx11-dev libxtst-dev pkg-config
   ```
4. Build:
   - Server: `cd server && qmake server.pro && make -j$(nproc)`
   - Client: `cd client && qmake client.pro && make -j$(nproc)`

## Development Workflow

1. Create a new branch: `git checkout -b my-feature`
2. Make your changes.
3. Build and test locally.
4. Commit with a clear message: `git commit -m "Add: brief description"`
5. Push: `git push origin my-feature`
6. Open a Pull Request.

## Reporting Issues

- Use the **Bug report** template for bugs.
- Use the **Feature request** template for new features.
- Before opening a new issue, search existing ones to avoid duplicates.

## Good First Issues

Issues labeled `good first issue` are great for newcomers. These are typically
self-contained and a good way to get familiar with the codebase.

## Code Style

- Follow the existing C++ and qmake conventions in the project.
- Keep changes focused — one feature or fix per PR.
- Add or update documentation when changing behavior.

## License

By contributing, you agree that your contributions will be licensed under the
GPL-3.0 License.