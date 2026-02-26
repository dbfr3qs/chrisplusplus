import * as path from 'path';
import * as fs from 'fs';
import { workspace, ExtensionContext, window } from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind,
} from 'vscode-languageclient/node';

let client: LanguageClient;

function findLspBinary(context: ExtensionContext): string | undefined {
    // 1. Check user setting
    const configPath = workspace.getConfiguration('chrisplusplus').get<string>('lspPath');
    if (configPath && fs.existsSync(configPath)) {
        return configPath;
    }

    // 2. Check common build locations relative to workspace folders
    const searchPaths: string[] = [];

    for (const folder of workspace.workspaceFolders ?? []) {
        const root = folder.uri.fsPath;
        searchPaths.push(
            path.join(root, 'build', 'chris-lsp'),
            path.join(root, 'cmake-build-debug', 'chris-lsp'),
            path.join(root, 'cmake-build-release', 'chris-lsp'),
        );
    }

    // 3. Check relative to the extension itself (for monorepo layouts)
    const extDir = context.extensionPath;
    searchPaths.push(
        path.join(extDir, '..', '..', 'build', 'chris-lsp'),
        path.join(extDir, '..', '..', '..', 'build', 'chris-lsp'),
    );

    for (const p of searchPaths) {
        if (fs.existsSync(p)) {
            return p;
        }
    }

    // 4. Check if chris-lsp is on PATH
    try {
        const { execFileSync } = require('child_process');
        const which = execFileSync('which', ['chris-lsp'], { encoding: 'utf-8' }).trim();
        if (which && fs.existsSync(which)) {
            return which;
        }
    } catch {}

    return undefined;
}

export function activate(context: ExtensionContext) {
    const lspPath = findLspBinary(context);

    if (!lspPath) {
        window.showWarningMessage(
            'chrisplusplus: Could not find chris-lsp binary. ' +
            'Build it with `cmake --build build --target chris-lsp` or set chrisplusplus.lspPath in settings.'
        );
        return;
    }

    const serverOptions: ServerOptions = {
        command: lspPath,
        transport: TransportKind.stdio,
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'chrisplusplus' }],
        synchronize: {
            fileEvents: workspace.createFileSystemWatcher('**/*.chr'),
        },
    };

    client = new LanguageClient(
        'chrisplusplus',
        'chrisplusplus Language Server',
        serverOptions,
        clientOptions
    );

    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
