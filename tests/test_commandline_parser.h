#ifndef TEST_COMMANDLINE_PARSER_H
#define TEST_COMMANDLINE_PARSER_H

#include <QObject>

class TestCommandlineParser : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testParseProcessEnumDirect();
    void testParseLatticeEnumDirect();
    void testValidOptionsSetParameters();
    void testInvalidOptionsRejected();

    void testJsonSourceMapsKeysCorrectly();
    void testJsonSourceNestedFlattening();
    void testApplyParameterValidAndInvalid();
    void testUnknownKeyRejected();
    void testCliOverridesFile();
    void testYamlSourceStub();
    void testConfigCallerPathUnknownKey();
    void testConfigCallerPathMissingAndMalformed();
    void testCliCallerPathGracefulErrors();
};

#endif // TEST_COMMANDLINE_PARSER_H
