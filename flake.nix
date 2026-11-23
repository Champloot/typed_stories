{
  description = "Терминальная игра на печать (C + ncurses)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      # nix develop (или direnv с `use flake`) — окружение для make
      devShells.${system}.default = pkgs.mkShell {
        # Инструменты сборки. gcc и make уже входят в mkShell (stdenv),
        # отдельно их добавлять не нужно.
        nativeBuildInputs = with pkgs; [
          pkg-config
          gdb
          clang-tools # clangd для подсказок в Zed (нужен compile_commands.json, см. bear)
          bear        # bear -- make  → compile_commands.json
          # make test: тесты проигрывают целые дни через эмулятор терминала pyte
          (python3.withPackages (ps: [ ps.pyte ]))
        ];

        # Библиотеки: заголовки и .so находит обёртка gcc сама
        # (-I…/include и -L…/lib подставляются автоматически), путь для pkg-config тоже.
        buildInputs = with pkgs; [
          ncurses
          # Для графики потом — раскомментировать одну:
          # raylib
          # SDL2
        ];
      };
    };
}
