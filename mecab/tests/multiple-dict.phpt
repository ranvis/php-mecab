--TEST--
Multiple user dictionary
--SKIPIF--
<?php
if (!extension_loaded('mecab')) {
    die('skip mecab extension is not loaded');
}
?>
--FILE--
<?php
$dicPath = getSysDicDirPath();
$indexerPath = getIndexerPath();

$userDicData = "辞書,,,5000,名詞,一般,*,*,*,*,辞書,ジショ,テスト";
$userDicPaths = [];
$userDicTextPath = tempnam(__DIR__, 'dic');
file_put_contents($userDicTextPath, $userDicData);
for ($i = 0; $i < 2; $i++) {
    $userDicPaths[] = $userDicPath = tempnam(__DIR__, 'dic');
    if (!runCmd([$indexerPath, '-d', $dicPath, '-f', 'utf-8', '-c', 'utf-8', '-u', $userDicPath, $userDicTextPath])) {
        error('Unable to prepare user dictionary.');
    }
}
unlink($userDicTextPath);

$mecab = new \Mecab\Tagger(['-u', implode(',', $userDicPaths)]);
$out = $mecab->parseToString("辞書");
echo (strpos($out, "テスト") !== false) ? 'OK' : 'Parse error: ' . $out;
$mecab = null;
foreach ($userDicPaths as $userDicPath) {
    unlink($userDicPath);
}

function error($msg)
{
    echo $msg;
    exit;
}

function runCmd(array $args, &$output = '')
{
    $cmd = [];
    foreach ($args as $arg) {
        $cmd[] = escapeshellarg($arg);
    }
    $cmd = implode(' ', $cmd);
    exec($cmd, $output, $status);
    $output = implode("\n", $output);
    return !$status;
}

function getSysDicDirPath()
{
    if (($dic = getenv('PHP_MECAB_TEST_DIC')) !== false) {
        return $dic;
    }
    if (!runCmd(['mecab-config', '--dicdir'], $dicBasePath)) {
        error('Unable to locate dictionary base path.');
    }
    foreach (scandir($dicBasePath) as $dicPath) {
        if (preg_match('/^\.\.?$/D', $dicPath)) {
            continue;
        }
        $dicPath = $dicBasePath . '/' . $dicPath;
        if (file_exists($dicPath. '/dicrc')) {
            return $dicPath;
        }
    }
    error('Unable to locate dictionary.');
}

function getIndexerPath()
{
    $indexerName = 'mecab-dict-index';
    if (($dir = getenv('PHP_MECAB_TEST_BIN_DIR')) !== false) {
        return $dir . '/' . $indexerName;
    }
    foreach (['/usr/libexec/mecab', '/usr/local/libexec/mecab'] as $binDir) {
        if (is_executable($binDir . '/' . $indexerName)) {
            return $binDir . '/' . $indexerName;
        }
    }
    return $indexerName;
}
?>
--EXPECT--
OK
