import './PinMapping.scss';
import React, { useState, useEffect, useContext } from 'react';
import { useTranslation } from 'react-i18next';
import { Container, Row, Col, Form, Button, Alert, Spinner, OverlayTrigger, Tooltip } from 'react-bootstrap';
import CustomSelect from '../Components/CustomSelect';
import Section from '../Components/Section';
import WebApi from '../Services/WebApi';
import { AppContext } from '../Contexts/AppContext';
import useProfilesStore from '../Store/useProfilesStore';
import { BUTTON_ACTIONS } from '../Data/Pins';
import { BUTTON_MASKS, DPAD_MASKS, getButtonLabels } from '../Data/Buttons';
import omit from 'lodash/omit';
import invert from 'lodash/invert';


const disabledOptions = [BUTTON_ACTIONS.RESERVED, BUTTON_ACTIONS.ASSIGNED_TO_ADDON];

const getMask = (maskArr, key) => maskArr.find(
    ({ label }) => label?.toUpperCase() === key.split('BUTTON_PRESS_')?.pop()
);

const options = Object.entries(BUTTON_ACTIONS)
    .filter(([, value]) => ![
        BUTTON_ACTIONS.NONE,
        BUTTON_ACTIONS.CUSTOM_BUTTON_COMBO,
        ...disabledOptions
    ].includes(value))
    .map(([key, value]) => {
        const buttonMask = getMask(BUTTON_MASKS, key);
        const dpadMask = getMask(DPAD_MASKS, key);
        return {
            label: key,
            value,
            type: buttonMask ? 'customButtonMask' : dpadMask ? 'customDpadMask' : 'action',
            customButtonMask: buttonMask?.value || 0,
            customDpadMask: dpadMask?.value || 0,
        };
    });

export default function UartInputPage() {
    const [pinMapping, setPinMapping] = useState({});
    const { t } = useTranslation('UartInputPage');
    const { t: tProto } = useTranslation('Proto');
    const [loading, setLoading] = useState(true);
    const [saving, setSaving] = useState(false);

    const noneOption = { label: t('none', 'None'), value: -1 };
    const virtualPinOptions = [
        noneOption,
        ...Array.from({ length: 64 }, (_, i) => ({ label: i.toString(), value: i }))
    ];
    const pinOptions = [
        noneOption,
        ...Array.from({ length: 30 }, (_, i) => ({ label: i.toString(), value: i }))
    ];
    const groupedOptions = [
        { label: t('groupButtons', 'Buttons'), options: options.filter(({ type }) => type !== 'action') },
        { label: t('groupActions', 'Actions'), options: options.filter(({ type }) => type === 'action') },
    ];
    const [error, setError] = useState(null);
    const [saveMessage, setSaveMessage] = useState('');

    // Stato per auto‑detect e verifica
    const [autoDetectLoading, setAutoDetectLoading] = useState(false);
    const [statusMessage, setStatusMessage] = useState(null);
    const [profileLabel, setProfileLabel] = useState('');
    
    // Configurazione unificata: include anche mappingEnabled
    const [baseConfig, setBaseConfig] = useState({
        enabled: false,
        baudRate: 115200,
        txPin: -1,      // default None
        rxPin: -1,      // default None
        mappingEnabled: false,
        remoteDisplayEnabled: false,
        analogEnabled: false,
        he_triggerEnabled: false,
        rotaryencoderEnabled: false,
    });

const [mappings, setMappings] = useState(() =>
    Array.from({ length: 30 }, (_, i) => ({
        virtualPin: -1,
        gpio: i,
    }))
);

    const [lastSourcePin, setLastSourcePin] = useState(null);

    // Store dei profili (per le action)
    const profiles = useProfilesStore((state) => state.profiles);
    const setProfilePin = useProfilesStore((state) => state.setProfilePin);
    const saveProfiles = useProfilesStore((state) => state.saveProfiles);
    const profileIndex = 0; // profilo principale (indice 0)

// Helper per ottenere i dati di un GPIO dallo store
    const getPinData = (gpio) => {
    const pinKey = `pin${gpio.toString().padStart(2, '0')}`;
    return profiles[profileIndex]?.[pinKey] || { action: BUTTON_ACTIONS.NONE, customButtonMask: 0, customDpadMask: 0 };
};

// Funzione per ottenere il valore selezionato (multi) per CustomSelect
const getMultiValue = (pinData) => {
    if (pinData.action === BUTTON_ACTIONS.NONE) return [];
    if (disabledOptions.includes(pinData.action)) {
        const actionKey = invert(BUTTON_ACTIONS)[pinData.action];
        return [{
            label: actionKey,
            value: pinData.action,
            type: 'action',
            customButtonMask: pinData.customButtonMask,
            customDpadMask: pinData.customDpadMask,
        }];
    }
    if (pinData.action === BUTTON_ACTIONS.CUSTOM_BUTTON_COMBO) {
        return options.filter(({ type, customButtonMask, customDpadMask }) =>
            (pinData.customButtonMask & customButtonMask && type === 'customButtonMask') ||
            (pinData.customDpadMask & customDpadMask && type === 'customDpadMask')
        );
    }
    return options.filter(option => option.value === pinData.action);
};

// Funzione per gestire il cambio di azione
const handleActionChange = (gpio, selected) => {
    const pinKey = `pin${gpio.toString().padStart(2, '0')}`;
    if (!selected || (Array.isArray(selected) && !selected.length)) {
        setProfilePin(profileIndex, pinKey, {
            action: BUTTON_ACTIONS.NONE,
            customButtonMask: 0,
            customDpadMask: 0,
        });
    } else if (Array.isArray(selected) && selected.length > 1) {
        const lastSelected = selected[selected.length - 1];
        if (lastSelected.type === 'action') {
            setProfilePin(profileIndex, pinKey, {
                action: lastSelected.value,
                customButtonMask: 0,
                customDpadMask: 0,
            });
        } else {
            const masks = selected.reduce(
                (acc, opt) => ({
                    customButtonMask: opt.type === 'customButtonMask' ? acc.customButtonMask ^ opt.customButtonMask : acc.customButtonMask,
                    customDpadMask: opt.type === 'customDpadMask' ? acc.customDpadMask ^ opt.customDpadMask : acc.customDpadMask,
                }),
                { customButtonMask: 0, customDpadMask: 0 }
            );
            setProfilePin(profileIndex, pinKey, {
                action: BUTTON_ACTIONS.CUSTOM_BUTTON_COMBO,
                ...masks,
            });
        }
    } else {
        setProfilePin(profileIndex, pinKey, {
            action: selected[0].value,
            customButtonMask: 0,
            customDpadMask: 0,
        });
    }
};

    const handleMappingPinChange = (index, selected) => {
        const newMappings = [...mappings];
        newMappings[index] = { ...newMappings[index], virtualPin: selected?.value ?? -1 };
        setMappings(newMappings);
        setSaveMessage('');
    };

    const { buttonLabels } = useContext(AppContext);
    const { buttonLabelType, swapTpShareLabels } = buttonLabels;
    const CURRENT_BUTTONS = getButtonLabels(buttonLabelType, swapTpShareLabels);
    const buttonNames = Object.fromEntries(
    Object.entries(CURRENT_BUTTONS).filter(([key]) => key !== 'label' && key !== 'value')
    );
    const actionNumberToKey = invert(BUTTON_ACTIONS);

    const getOptionLabel = (option) => {
    const labelKey = option.label?.split('BUTTON_PRESS_')?.pop();
    return (labelKey && buttonNames[labelKey]) || tProto(`GpioAction.${option.label}`);
};

    const getActionDisplayName = (actionNumber) => {
    if (actionNumber === undefined || actionNumber === null) return t('none', 'None');
    if (actionNumber === BUTTON_ACTIONS.NONE) return t('none', 'None');
    if (actionNumber === BUTTON_ACTIONS.ASSIGNED_TO_ADDON) return t('assignedToAddon', 'Assigned to Addon');
    if (actionNumber === BUTTON_ACTIONS.RESERVED) return t('reserved', 'Reserved');
    if (actionNumber === BUTTON_ACTIONS.CUSTOM_BUTTON_COMBO) return t('customCombo', 'Custom Combo');

    const key = actionNumberToKey[actionNumber];
    if (key && key.startsWith('BUTTON_PRESS_')) {
        const buttonKey = key.replace('BUTTON_PRESS_', '').toUpperCase();
        // Usa il nome tradotto da buttonNames se esiste, altrimenti il nome originale
        return buttonNames[buttonKey] || key;
        }
    return actionNumber.toString();
    };

    const fetchProfiles = useProfilesStore((state) => state.fetchProfiles);

    useEffect(() => {
    const loadData = async () => {
        try {
            setLoading(true);
            await fetchProfiles();
            const base = await WebApi.getUartConfig();
            const pm = await WebApi.getPinMappings();
            setPinMapping(pm ?? {});
            
            setBaseConfig({
                enabled: base.enabled ?? false,
                baudRate: base.baudRate ?? 115200,
                txPin: base.txPin ?? -1,
                rxPin: base.rxPin ?? -1,
                remoteDisplayEnabled: base.remoteDisplayEnabled ?? false,
                mappingEnabled: base.mappingEnabled ?? false,
                analogEnabled: base.analogEnabled ?? false,
                he_triggerEnabled: base.he_triggerEnabled ?? false,
                rotaryencoderEnabled: base.rotaryencoderEnabled ?? false,
            });

            if (base.enabled) {
                const mappingData = await WebApi.getUartMapping();

                if (mappingData.mappings?.length === 30) {
                    setMappings(
                      mappingData.mappings.map(m => ({
                           virtualPin: m.virtualPin ?? -1,
                    }))
                );
            }
        }        
        } catch (err) {
            console.error(err);
            setError(t('loadError', 'Failed to load UART configuration'));
        } finally {
            setLoading(false);
        }
    };

    loadData();
}, [t]);

    const handleBaseChange = (e) => {
        const { name, value, type, checked } = e.target;
        setBaseConfig(prev => ({ ...prev, [name]: type === 'checkbox' ? checked : value }));
        setSaveMessage('');
    };

    const handleSave = async () => {
        setSaving(true);
        setError(null);
        setSaveMessage('');
        try {
            await WebApi.setUartConfig({
                enabled: baseConfig.enabled,
                baudRate: baseConfig.baudRate,
                txPin: baseConfig.txPin,
                rxPin: baseConfig.rxPin,
                remoteDisplayEnabled: baseConfig.remoteDisplayEnabled,
                mappingEnabled: baseConfig.mappingEnabled,
                analogEnabled: baseConfig.analogEnabled,
                he_triggerEnabled: baseConfig.he_triggerEnabled,
                rotaryencoderEnabled: baseConfig.rotaryencoderEnabled
            });
            if (baseConfig.mappingEnabled) {
                const cleanMappings = mappings.map((m, index) => ({
                virtualPin: m.virtualPin,
             gpio: index,
            }));
                await WebApi.setUartMapping({ mappings: cleanMappings });
            }
            await saveProfiles();
            setSaveMessage(t('Common:saved-success-message'));
            setTimeout(() => setSaveMessage(''), 3000);
        } catch (err) {
            console.error(err);
            setError(t('Common:saved-error-message'));
        } finally {
            setSaving(false);
        }
    };

    // Nuove funzioni per auto‑detect e verifica stato
    const handleAutoDetect = async () => {
        setAutoDetectLoading(true);
        setStatusMessage(null);
        setError(null);
        try {
            const result = await WebApi.autoDetectUart(baseConfig.baudRate);
            if (result.success) {
                // Aggiorna i pin RX/TX con i valori rilevati
                setBaseConfig(prev => ({
                    ...prev,
                    rxPin: result.rxPin,
                    txPin: result.txPin,
                }));
                setStatusMessage({ type: 'success', text: t('autoDetectSuccess', 'Auto‑detect successful! RX={rx}, TX={tx}. Please save and restart.', { rx: result.rxPin, tx: result.txPin }) });
            } else {
                setStatusMessage({ type: 'danger', text: result.error || t('autoDetectFailed', 'Auto‑detect failed.') });
            }
        } catch (err) {
            console.error(err);
            setStatusMessage({ type: 'danger', text: t('autoDetectError', 'Communication error during auto‑detect.') });
        } finally {
            setAutoDetectLoading(false);
            // Il messaggio rimane visibile finché non viene cambiato
        }
    };

    const handleCheckStatus = async () => {
        setStatusMessage(null);
        setError(null);
        try {
            const status = await WebApi.getUartStatus();
            setStatusMessage({ type: 'info', text: status.status });
        } catch (err) {
            console.error(err);
            setStatusMessage({ type: 'danger', text: t('statusError', 'Failed to retrieve UART status.') });
        }
    };

    if (loading) {
        return (
            <Container className="text-center mt-5">
                <Spinner animation="border" variant="primary" />
            </Container>
        );
    }

    const leftMappings = mappings.slice(0, 15);
    const rightMappings = mappings.slice(15, 30);

    return (
        <Container fluid>
            <Section title={t('title', 'UART Configuration')}>
                <p className="text-muted">{t('description', 'Configure external (ESP32) serial input for your arcade stick.')}</p>
                {error && <Alert variant="danger">{error}</Alert>}
                {statusMessage && (
                    <Alert variant={statusMessage.type} onClose={() => setStatusMessage(null)} dismissible>
                        {statusMessage.text}
                    </Alert>
                )}

                <Form>
                    <Row className="mb-3">
                        <Form.Label column sm={3}>{t('enableAddon', 'Enable UART Input Addon')}</Form.Label>
                        <Col sm={9}>
                            <Form.Check
                                type="switch"
                                name="enabled"
                                checked={baseConfig.enabled}
                                onChange={handleBaseChange}
                                label={baseConfig.enabled ? t('enabledLabel', 'Enabled') : t('disabledLabel', 'Disabled')}
                            />
                        </Col>
                    </Row>
                    <Row className="mb-3">
                        <Form.Label column sm={3}>{t('baudRate', 'Baud Rate')}</Form.Label>
                        <Col sm={9}>
                            <Form.Select
                                name="baudRate"
                                value={baseConfig.baudRate}
                                onChange={handleBaseChange}
                                disabled={!baseConfig.enabled}
                                className="form-select-sm"
                            >
                                {[9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000, 1500000, 2000000, 3000000, 4000000].map(br => (
                                    <option key={br} value={br}>{br}</option>
                                ))}
                            </Form.Select>
                        </Col>
                    </Row>
                    <Row className="mb-3">
                        <Form.Label column sm={3}>{t('txPin', 'GPIO TX Pin')}</Form.Label>
                        <Col sm={9}>
                            <CustomSelect
                                options={pinOptions}
                                value={pinOptions.find(opt => opt.value === baseConfig.txPin) || pinOptions[0]}
                                onChange={(opt) => handleBaseChange({ target: { name: 'txPin', value: opt?.value ?? -1 } })}
                                isClearable={false}
                                placeholder={t('pinSelectPlaceholder', 'Select pin')}
                            />
                        </Col>
                    </Row>
                    <Row className="mb-3">
                        <Form.Label column sm={3}>{t('rxPin', 'GPIO RX Pin')}</Form.Label>
                        <Col sm={9}>
                            <CustomSelect
                                options={pinOptions}
                                value={pinOptions.find(opt => opt.value === baseConfig.rxPin) || pinOptions[0]}
                                onChange={(opt) => handleBaseChange({ target: { name: 'rxPin', value: opt?.value ?? -1 } })}
                                isClearable={false}
                                placeholder={t('pinSelectPlaceholder', 'Select pin')}
                            />
                        </Col>
                    </Row>
                </Form>

                <hr />
                
                <Row className="mb-3">
                    <Form.Label column sm={3}>
                        {t('enableRemoteDisplay', 'Enable Remote Display')}
                    </Form.Label>
                    <Col sm={9}>
                        <Form.Check
                            type="switch"
                            name="remoteDisplayEnabled"
                            checked={baseConfig.remoteDisplayEnabled || false}
                            onChange={(e) =>
                                setBaseConfig(prev => ({
                                    ...prev,
                                    remoteDisplayEnabled: e.target.checked
                                }))
                            }
                            label={baseConfig.remoteDisplayEnabled ? t('enabledLabel', 'Enabled') : t('disabledLabel', 'Disabled')}
                        />
                    </Col>
                </Row>

                <Row className="mb-4">
                    <Form.Label column sm={3}>{t('enableMapping', 'Enable pin mapping')}</Form.Label>
                    <Col sm={9}>
                        <Form.Check
                            type="switch"
                            name="mappingEnabled"
                            checked={baseConfig.mappingEnabled}
                            onChange={handleBaseChange}
                            label={baseConfig.mappingEnabled ? t('mappingEnabled', 'Mapping active') : t('mappingDisabled', 'Mapping disabled')}
                            disabled={!baseConfig.enabled}
                        />
                    </Col>
                </Row>

                {/* Pulsanti auto‑detect e verifica stato */}
                <Row className="mb-4">
                    <Col sm={{ span: 9, offset: 3 }}>
                        <Button
                            variant="primary"
                            onClick={handleAutoDetect}
                            disabled={autoDetectLoading || !baseConfig.enabled}
                            className="me-2"
                        >
                            {autoDetectLoading ? <Spinner as="span" animation="border" size="sm" /> : t('autoDetectButton', 'Auto‑detect UART')}
                        </Button>
                        <Button
                            variant="secondary"
                            onClick={handleCheckStatus}
                            disabled={!baseConfig.enabled}
                        >
                            {t('checkStatusButton', 'Check UART status')}
                        </Button>
                    </Col>
                </Row>
            </Section>

            {baseConfig.mappingEnabled && (
                <Section title={t('mappingTable.title', 'Source Pin Mapping')}>
                    <Row>
                        <Col md={2}>
                            <div className="mb-4">
                                <h5>{t('profile.title', 'Profile')}</h5>
                                <Form.Group className="mb-3">
                                    <Form.Label>{t('profile.nameLabel', 'Profile name')}</Form.Label>
                                    <Form.Control
                                        type="text"
                                        value={profileLabel}
                                        onChange={(e) => setProfileLabel(e.target.value)}
                                        maxLength={16}
                                        className="form-control-sm"
                                    />
                                </Form.Group>
                                <Button variant="outline-secondary" disabled className="w-100">
                                    {t('profile.addButton', 'Add profile (inactive)')}
                                </Button>
                            </div>
                            <div>
                                <h5>{t('pinViewer.title', 'Source Pin Viewer')}</h5>
                                <div className="text-center">
                                    <OverlayTrigger overlay={<Tooltip>{t('pinViewer.tooltip', 'Displays the last received source pin')}</Tooltip>}>
                                        <Button variant="outline-secondary" disabled>
                                            🎮 {t('pinViewer.buttonLabel', 'Pin Viewer')}
                                        </Button>
                                    </OverlayTrigger>
                                    {lastSourcePin !== null && (
                                        <div className="alert alert-info mt-3">
                                            {t('pinViewer.lastPinLabel', 'Last pin:')} {lastSourcePin}
                                        </div>
                                    )}
                                    <div className="mb-4">
                                    <h5>{t('uartAddonRouting', 'UART Addon Routing')}</h5>

                                    <Form.Check
                                        className="mb-2"
                                        type="switch"
                                        label={t('enableAnalogUartInputs', 'Enable Analog UART Inputs')}
                                        checked={baseConfig.analogEnabled || false}
                                        disabled={!baseConfig.enabled}
                                        onChange={(e) =>
                                            setBaseConfig(prev => ({
                                                ...prev,
                                                analogEnabled: e.target.checked
                                            }))
                                        }
                                    />

                                    <Form.Check
                                        className="mb-2"
                                        type="switch"
                                        label={t('enableHeTriggerUartInputs', 'Enable HE Trigger UART Inputs')}
                                        checked={baseConfig.he_triggerEnabled || false}
                                        disabled={!baseConfig.enabled}
                                        onChange={(e) =>
                                            setBaseConfig(prev => ({
                                                ...prev,
                                                he_triggerEnabled: e.target.checked
                                            }))
                                        }
                                    />

                                    <Form.Check
                                        className="mb-2"
                                        type="switch"
                                        label={t('enableRotaryEncoderUartInputs', 'Enable Rotary Encoder UART Inputs')}
                                        checked={baseConfig.rotaryencoderEnabled || false}
                                        disabled={!baseConfig.enabled}
                                        onChange={(e) =>
                                            setBaseConfig(prev => ({
                                                ...prev,
                                                rotaryencoderEnabled: e.target.checked
                                            }))
                                        }
                                    />
                                </div>
                                </div>
                            </div>
                        </Col>

                        <Col md={10}>
                            <Row>
                                <Col md={6}>
                                    <div className="mb-2">
                                        <strong>{t('mappingTable.sourcePinHeader', 'GPIO → VirtualPin → Action')}</strong>
                                        <span style={{ float: 'right' }}><strong>{t('mappingTable.autoMapHeader', 'Auto‑map')}</strong></span>
                                    </div>
                                    {leftMappings.map((mapping, idx) => (
                                        <div key={idx} className="d-flex align-items-center mb-2">

    <div className="d-flex flex-shrink-0" style={{ width: '2.5rem' }}>
						<label>GP{idx}</label>
					</div>

    <div style={{ width: '100px', marginLeft: '8px' }}>
        <CustomSelect
            options={virtualPinOptions}
            value={
                virtualPinOptions.find(
                    o => o.value === mapping.virtualPin
                ) ?? virtualPinOptions[0]
            }
            onChange={(opt) =>
                handleMappingPinChange(idx, opt)
            }
            isClearable={false}
        />
    </div>

    <div style={{ flex: 1, marginLeft: '8px' }}>
    <CustomSelect
        isClearable
        isMulti={!disabledOptions.includes(getPinData(idx).action)}
        options={groupedOptions}
        isDisabled={disabledOptions.includes(getPinData(idx).action)}
        getOptionLabel={getOptionLabel}
        onChange={(opt) => handleActionChange(idx, opt)}
        value={getMultiValue(getPinData(idx))}
        placeholder={t('none', 'None')}
    />
</div>

    <div
        style={{
            width: '40px',
            marginLeft: '8px',
            textAlign: 'center'
        }}
    >
        <OverlayTrigger
            overlay={
                <Tooltip>
                    {t(
                        'mappingTable.autoMapTooltip',
                        'Click to map with your input'
                    )}
                </Tooltip>
            }
        >
            <Button
                variant="outline-secondary"
                size="sm"
                disabled
            >
                🎮
            </Button>
        </OverlayTrigger>
    </div>

</div>
                                        ))}
                                </Col>
                                <Col md={6}>
                                    <div className="mb-2">
                                        <strong>{t('mappingTable.sourcePinHeader', 'GPIO → Virtual Pin → Action')}</strong>
                                        <span style={{ float: 'right' }}><strong>{t('mappingTable.autoMapHeader', 'Auto‑map')}</strong></span>
                                    </div>
                                    {rightMappings.map((mapping, idx) => {
                                        const globalIdx = idx + 15;
                                        return (
                                            <div
    key={globalIdx}
    className="d-flex align-items-center mb-2"
>

    <div className="d-flex flex-shrink-0" style={{ width: '2.5rem' }}>
						<label>GP{globalIdx}</label>
					</div>

    <div
        style={{
            width: '100px',
            marginLeft: '8px'
        }}
    >
        <CustomSelect
            options={virtualPinOptions}
            value={
                virtualPinOptions.find(
                    o => o.value === mapping.virtualPin
                ) ?? virtualPinOptions[0]
            }
            onChange={(opt) =>
                handleMappingPinChange(globalIdx, opt)
            }
            isClearable={false}
        />
    </div>

    <div style={{ flex: 1, marginLeft: '8px' }}>
    <CustomSelect
        isClearable
        isMulti={!disabledOptions.includes(getPinData(globalIdx).action)}
        options={groupedOptions}
        isDisabled={disabledOptions.includes(getPinData(globalIdx).action)}
        getOptionLabel={getOptionLabel}
        onChange={(opt) => handleActionChange(globalIdx, opt)}
        value={getMultiValue(getPinData(globalIdx))}
        placeholder={t('none', 'None')}
    />
</div>

    <div
        style={{
            width: '40px',
            marginLeft: '8px',
            textAlign: 'center'
        }}
    >
        <OverlayTrigger
            overlay={
                <Tooltip>
                    {t(
                        'mappingTable.autoMapTooltip',
                        'Click to map with your input'
                    )}
                </Tooltip>
            }
        >
            <Button
                variant="outline-secondary"
                size="sm"
                disabled
            >
                🎮
            </Button>
        </OverlayTrigger>
    </div>

</div>
                                        );
                                    })}
                                </Col>
                            </Row>
                        </Col>
                    </Row>
                </Section>
            )}

            <div className="d-flex justify-content-end mt-4">
                <Button variant="primary" onClick={handleSave} disabled={saving}>
                    {saving ? (t('saving', 'Saving...')) : t('saveButton', 'Save & reconfigure')}
                </Button>
                {saveMessage && <span className="alert alert-info ms-2">{saveMessage}</span>}
            </div>
        </Container>
    );
}