#include "speciesform.h"
#include "wheelignorefilter.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

QCheckBox* makeCheckBox(const QString& text, const QString& key = QString())
{
    auto* box = new QCheckBox(text);
    if (!key.isEmpty())
        box->setProperty("key", key);
    return box;
}

QCheckBox* makeMonthCheckBox(int month)
{
    auto* box = new QCheckBox(QStringLiteral("%1月").arg(month));
    box->setProperty("month", month);
    return box;
}

int comboIndexByKey(const QComboBox* combo, const QString& key)
{
    const int index = combo->findData(key);
    return index >= 0 ? index : 0;
}

void addChecksToGrid(QGridLayout* grid, const QStringList& keys,
                     QVector<QCheckBox*>& checks,
                     const std::function<QString(const QString&)>& labelFor)
{
    const int columns = 4;
    for (int i = 0; i < keys.size(); ++i) {
        QCheckBox* box = makeCheckBox(labelFor(keys.at(i)), keys.at(i));
        checks.append(box);
        grid->addWidget(box, i / columns, i % columns);
    }
}

} // namespace

SpeciesForm::SpeciesForm(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    m_stack->addWidget(buildHintPage());
    m_stack->addWidget(buildFormPage());
    showNode(0);
}

void SpeciesForm::setDocument(TaxonomyDocument* document)
{
    m_document = document;
    showNode(m_nodeId);
}

void SpeciesForm::setDataDir(const QString& path)
{
    m_dataDir = path;
}

QStringList SpeciesForm::sessionCopiedPhotoFiles() const
{
    return m_copiedPhotosThisSession;
}

void SpeciesForm::showNode(int id)
{
    m_nodeId = id;
    const TaxonNode* n = m_document ? m_document->node(id) : nullptr;
    if (!n) {
        m_hintLabel->setText(QStringLiteral("在左侧选择分类树中的节点。\n\n"
                                             "只有“种”或“亚种”节点可以填写物种资料。"));
        m_stack->setCurrentIndex(0);
        return;
    }

    m_pathLabel->setText(QStringLiteral("%1（%2）")
                             .arg(m_document->displayPathOf(id),
                                  TaxonRanks::displayName(n->rank)));

    if (!TaxonRanks::canHostPlantInfo(n->rank)) {
        m_hintLabel->setText(QStringLiteral("当前节点：%1（%2）\n\n"
                                             "这是纯分类层级，不保存资料。\n"
                                             "请先在工具栏点击“添加下级分类”，"
                                             "逐级添加直到建立“种 / 亚种”。")
                                 .arg(n->name, TaxonRanks::displayName(n->rank)));
        m_stack->setCurrentIndex(0);
        return;
    }

    m_stack->setCurrentIndex(1);
    if (m_formScroll)
        m_formScroll->verticalScrollBar()->setValue(0);
    m_loading = true;
    if (n->hasInfo) {
        populateFromInfo(n->info);
    } else {
        populateFromInfo(SpeciesInfo());
    }
    m_loading = false;
}

QWidget* SpeciesForm::buildHintPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    m_hintLabel = new QLabel(page);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet(QStringLiteral("font-size: 15px; color: #555;"));
    layout->addStretch();
    layout->addWidget(m_hintLabel);
    layout->addStretch();
    return page;
}

QWidget* SpeciesForm::buildFormPage()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_formScroll = scroll;

    auto* content = new QWidget;
    auto* outer = new QVBoxLayout(content);

    m_pathLabel = new QLabel(content);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    outer->addWidget(m_pathLabel);

    // ---------------- 植物照片（点植物后先看这里的大图） ----------------
    auto* photoBox = new QGroupBox(
        QStringLiteral("植物照片（点击缩略图可切换大图）"), content);
    auto* photoLayout = new QVBoxLayout(photoBox);

    m_photoPreview = new QLabel(photoBox);
    m_photoPreview->setObjectName(QStringLiteral("photoPreview"));
    m_photoPreview->setAlignment(Qt::AlignCenter);
    m_photoPreview->setMinimumHeight(220);
    m_photoPreview->setStyleSheet(
        QStringLiteral("QLabel#photoPreview {"
                       "  border: 1px solid #b0b0b0;"
                       "  background: #f8f8f8;"
                       "  color: #777777;"
                       "  font-size: 14px; }"));
    m_photoPreview->setText(QStringLiteral("暂无照片\n\n点击左侧分类树中的植物后，"
                                           "可在这里看到它的图片。"));
    photoLayout->addWidget(m_photoPreview);

    m_photoList = new QListWidget(photoBox);
    m_photoList->setIconSize(QSize(88, 88));
    m_photoList->setViewMode(QListView::IconMode);
    m_photoList->setResizeMode(QListView::Adjust);
    m_photoList->setMovement(QListView::Static);
    m_photoList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_photoList->setMinimumHeight(104);
    m_photoList->setToolTip(QStringLiteral("选中即切换上方大图；"
                                           "可多选后从资料中移除。"));
    photoLayout->addWidget(m_photoList);

    auto* photoButtons = new QHBoxLayout;
    auto* addPhotoButton = new QPushButton(QStringLiteral("添加照片…"), photoBox);
    auto* removePhotoButton = new QPushButton(QStringLiteral("从资料移除"), photoBox);
    auto* openPhotoButton = new QPushButton(QStringLiteral("打开原图"), photoBox);
    photoButtons->addWidget(addPhotoButton);
    photoButtons->addWidget(removePhotoButton);
    photoButtons->addWidget(openPhotoButton);
    photoButtons->addStretch();
    photoLayout->addLayout(photoButtons);
    outer->addWidget(photoBox);

    connect(addPhotoButton, &QPushButton::clicked, this, &SpeciesForm::addPhotos);
    connect(removePhotoButton, &QPushButton::clicked, this, &SpeciesForm::removeSelectedPhotos);
    connect(openPhotoButton, &QPushButton::clicked, this, &SpeciesForm::openSelectedPhoto);
    connect(m_photoList, &QListWidget::currentItemChanged,
            this, &SpeciesForm::onPhotoSelectionChanged);

    // ---------------- 身份与描述 ----------------
    auto* identityBox = new QGroupBox(QStringLiteral("身份与描述"), content);
    auto* identityForm = new QFormLayout(identityBox);

    m_latinEdit = new QLineEdit(identityBox);
    m_latinEdit->setPlaceholderText(QStringLiteral("例：Rosa rugosa（全库唯一，不能重复）"));
    identityForm->addRow(QStringLiteral("拉丁学名："), m_latinEdit);

    auto* aliasEditor = new QWidget(identityBox);
    auto* aliasLayout = new QVBoxLayout(aliasEditor);
    aliasLayout->setContentsMargins(0, 0, 0, 0);
    m_aliasList = new QListWidget(aliasEditor);
    m_aliasList->setMinimumHeight(70);
    m_aliasList->setMaximumHeight(110);
    m_aliasList->setToolTip(QStringLiteral("每个别名单独保存一条"));
    aliasLayout->addWidget(m_aliasList);

    auto* aliasButtons = new QHBoxLayout;
    auto* addAliasButton = new QPushButton(QStringLiteral("添加别名…"), aliasEditor);
    auto* removeAliasButton = new QPushButton(QStringLiteral("删除选中"), aliasEditor);
    aliasButtons->addWidget(addAliasButton);
    aliasButtons->addWidget(removeAliasButton);
    aliasButtons->addStretch();
    aliasLayout->addLayout(aliasButtons);
    identityForm->addRow(QStringLiteral("别名 / 俗名："), aliasEditor);

    connect(addAliasButton, &QPushButton::clicked, this, &SpeciesForm::addAlias);
    connect(removeAliasButton, &QPushButton::clicked,
            this, &SpeciesForm::removeSelectedAlias);

    m_descriptionEdit = new QPlainTextEdit(identityBox);
    m_descriptionEdit->setPlaceholderText(QStringLiteral("在这里填写该物种的形态、分布、养护等自定义描述…"));
    m_descriptionEdit->setMinimumHeight(110);
    identityForm->addRow(QStringLiteral("自定义描述："), m_descriptionEdit);
    outer->addWidget(identityBox);

    // ---------------- 环境需求 ----------------
    auto* envBox = new QGroupBox(QStringLiteral("环境需求"), content);
    auto* envGrid = new QGridLayout(envBox);
    int row = 0;

    envGrid->addWidget(new QLabel(QStringLiteral("光照："), envBox), row, 0);
    m_lightCombo = new QComboBox(envBox);
    m_lightCombo->addItem(lightLabel(LightPreference::Unknown), lightToKey(LightPreference::Unknown));
    m_lightCombo->addItem(lightLabel(LightPreference::FullSun), lightToKey(LightPreference::FullSun));
    m_lightCombo->addItem(lightLabel(LightPreference::HalfSun), lightToKey(LightPreference::HalfSun));
    m_lightCombo->addItem(lightLabel(LightPreference::HalfShade), lightToKey(LightPreference::HalfShade));
    m_lightCombo->addItem(lightLabel(LightPreference::Shade), lightToKey(LightPreference::Shade));
    envGrid->addWidget(m_lightCombo, row, 1);

    envGrid->addWidget(new QLabel(QStringLiteral("水分："), envBox), row, 2);
    m_waterCombo = new QComboBox(envBox);
    m_waterCombo->addItem(waterLabel(WaterPreference::Unknown), waterToKey(WaterPreference::Unknown));
    m_waterCombo->addItem(waterLabel(WaterPreference::Dry), waterToKey(WaterPreference::Dry));
    m_waterCombo->addItem(waterLabel(WaterPreference::Moderate), waterToKey(WaterPreference::Moderate));
    m_waterCombo->addItem(waterLabel(WaterPreference::Moist), waterToKey(WaterPreference::Moist));
    m_waterCombo->addItem(waterLabel(WaterPreference::Aquatic), waterToKey(WaterPreference::Aquatic));
    envGrid->addWidget(m_waterCombo, row, 3);
    ++row;

    envGrid->addWidget(new QLabel(QStringLiteral("温度范围："), envBox), row, 0);
    auto* tempMinBox = new QSpinBox(envBox);
    auto* tempMaxBox = new QSpinBox(envBox);
    for (auto* spin : { tempMinBox, tempMaxBox }) {
        spin->setRange(-100, 100);
        spin->setSpecialValueText(QStringLiteral("未填写"));
        spin->setSuffix(QStringLiteral(" ℃"));
    }
    envGrid->addWidget(tempMinBox, row, 1);
    envGrid->addWidget(tempMaxBox, row, 2);
    m_tempMin = tempMinBox;
    m_tempMax = tempMaxBox;
    ++row;

    envGrid->addWidget(new QLabel(QStringLiteral("空气湿度："), envBox), row, 0);
    auto* humidityMin = new QSpinBox(envBox);
    auto* humidityMax = new QSpinBox(envBox);
    for (auto* spin : { humidityMin, humidityMax }) {
        spin->setRange(0, 100);
        spin->setSpecialValueText(QStringLiteral("未填写"));
        spin->setSuffix(QStringLiteral(" %"));
    }
    envGrid->addWidget(humidityMin, row, 1);
    envGrid->addWidget(humidityMax, row, 2);
    m_humidityMin = humidityMin;
    m_humidityMax = humidityMax;
    ++row;

    envGrid->addWidget(new QLabel(QStringLiteral("土壤 pH："), envBox), row, 0);
    auto* phMin = new QDoubleSpinBox(envBox);
    auto* phMax = new QDoubleSpinBox(envBox);
    for (auto* spin : { phMin, phMax }) {
        spin->setRange(0.0, 14.0);
        spin->setSpecialValueText(QStringLiteral("未填写"));
        spin->setDecimals(1);
        spin->setSingleStep(0.1);
    }
    envGrid->addWidget(phMin, row, 1);
    envGrid->addWidget(phMax, row, 2);
    m_phMin = phMin;
    m_phMax = phMax;
    ++row;

    envGrid->addWidget(new QLabel(QStringLiteral("土壤类型："), envBox), row, 0);
    QWidget* soilWidget = new QWidget(envBox);
    auto* soilGrid = new QGridLayout(soilWidget);
    soilGrid->setContentsMargins(0, 0, 0, 0);
    addChecksToGrid(soilGrid, predefinedSoilTypes(), m_soilChecks, &soilTypeLabel);
    envGrid->addWidget(soilWidget, row, 1, 1, 3);
    ++row;

    envGrid->addWidget(new QLabel(QStringLiteral("耐寒区(USDA)："), envBox), row, 0);
    auto* zoneLow = new QSpinBox(envBox);
    auto* zoneHigh = new QSpinBox(envBox);
    for (auto* spin : { zoneLow, zoneHigh }) {
        spin->setRange(0, 13);
        spin->setSpecialValueText(QStringLiteral("未知"));
    }
    envGrid->addWidget(zoneLow, row, 1);
    envGrid->addWidget(zoneHigh, row, 2);
    m_zoneLow = zoneLow;
    m_zoneHigh = zoneHigh;
    ++row;

    outer->addWidget(envBox);

    // ---------------- 生长形态 ----------------
    auto* growthBox = new QGroupBox(QStringLiteral("生长形态"), content);
    auto* growthForm = new QFormLayout(growthBox);

    m_habitCombo = new QComboBox(growthBox);
    for (int i = 0; i <= static_cast<int>(GrowthHabit::Other); ++i) {
        const auto value = static_cast<GrowthHabit>(i);
        m_habitCombo->addItem(habitLabel(value), habitToKey(value));
    }
    growthForm->addRow(QStringLiteral("生长习性："), m_habitCombo);

    m_lifecycleCombo = new QComboBox(growthBox);
    for (int i = 0; i <= static_cast<int>(LifeCycle::Perennial); ++i) {
        const auto value = static_cast<LifeCycle>(i);
        m_lifecycleCombo->addItem(lifecycleLabel(value), lifecycleToKey(value));
    }
    growthForm->addRow(QStringLiteral("生命周期："), m_lifecycleCombo);

    m_foliageCombo = new QComboBox(growthBox);
    for (int i = 0; i <= static_cast<int>(FoliageType::Deciduous); ++i) {
        const auto value = static_cast<FoliageType>(i);
        m_foliageCombo->addItem(foliageLabel(value), foliageToKey(value));
    }
    growthForm->addRow(QStringLiteral("叶型："), m_foliageCombo);

    m_growthRateCombo = new QComboBox(growthBox);
    for (int i = 0; i <= static_cast<int>(GrowthRate::Fast); ++i) {
        const auto value = static_cast<GrowthRate>(i);
        m_growthRateCombo->addItem(growthRateLabel(value), growthRateToKey(value));
    }
    growthForm->addRow(QStringLiteral("生长速度："), m_growthRateCombo);

    auto* sizeWidget = new QWidget(growthBox);
    auto* sizeLayout = new QGridLayout(sizeWidget);
    sizeLayout->setContentsMargins(0, 0, 0, 0);

    auto* heightMin = new QSpinBox(growthBox);
    auto* heightMax = new QSpinBox(growthBox);
    auto* spreadMin = new QSpinBox(growthBox);
    auto* spreadMax = new QSpinBox(growthBox);
    for (auto* spin : { heightMin, heightMax, spreadMin, spreadMax }) {
        spin->setRange(0, 50000);
        spin->setSuffix(QStringLiteral(" cm"));
        spin->setSpecialValueText(QStringLiteral("未填"));
    }
    m_heightMin = heightMin;
    m_heightMax = heightMax;
    m_spreadMin = spreadMin;
    m_spreadMax = spreadMax;

    sizeLayout->addWidget(new QLabel(QStringLiteral("株高："), growthBox), 0, 0);
    sizeLayout->addWidget(heightMin, 0, 1);
    sizeLayout->addWidget(new QLabel(QStringLiteral(" ~ "), growthBox), 0, 2);
    sizeLayout->addWidget(heightMax, 0, 3);
    sizeLayout->addWidget(new QLabel(QStringLiteral("冠幅："), growthBox), 1, 0);
    sizeLayout->addWidget(spreadMin, 1, 1);
    sizeLayout->addWidget(new QLabel(QStringLiteral(" ~ "), growthBox), 1, 2);
    sizeLayout->addWidget(spreadMax, 1, 3);
    growthForm->addRow(QStringLiteral("尺寸范围："), sizeWidget);

    auto* propagationWidget = new QWidget(growthBox);
    auto* propagationGrid = new QGridLayout(propagationWidget);
    propagationGrid->setContentsMargins(0, 0, 0, 0);
    addChecksToGrid(propagationGrid, predefinedPropagationMethods(), m_propagationChecks,
                    &propagationLabel);
    growthForm->addRow(QStringLiteral("繁殖方式："), propagationWidget);

    auto* usageWidget = new QWidget(growthBox);
    auto* usageGrid = new QGridLayout(usageWidget);
    usageGrid->setContentsMargins(0, 0, 0, 0);
    addChecksToGrid(usageGrid, predefinedUsageTags(), m_usageChecks, &usageTagLabel);
    growthForm->addRow(QStringLiteral("观赏/用途："), usageWidget);
    outer->addWidget(growthBox);

    // ---------------- 物候 ----------------
    auto* phenologyBox = new QGroupBox(QStringLiteral("物候（每年重复的月份）"), content);
    auto* phenologyForm = new QFormLayout(phenologyBox);

    auto* bloomWidget = new QWidget(phenologyBox);
    auto* bloomGrid = new QGridLayout(bloomWidget);
    bloomGrid->setContentsMargins(0, 0, 0, 0);
    for (int month = 1; month <= 12; ++month) {
        QCheckBox* box = makeMonthCheckBox(month);
        m_bloomChecks.append(box);
        bloomGrid->addWidget(box, (month - 1) / 6, (month - 1) % 6);
    }
    phenologyForm->addRow(QStringLiteral("花期："), bloomWidget);

    auto* fruitWidget = new QWidget(phenologyBox);
    auto* fruitGrid = new QGridLayout(fruitWidget);
    fruitGrid->setContentsMargins(0, 0, 0, 0);
    for (int month = 1; month <= 12; ++month) {
        QCheckBox* box = makeMonthCheckBox(month);
        m_fruitChecks.append(box);
        fruitGrid->addWidget(box, (month - 1) / 6, (month - 1) % 6);
    }
    phenologyForm->addRow(QStringLiteral("果期："), fruitWidget);
    outer->addWidget(phenologyBox);

    // ---------------- 扩展属性 ----------------
    auto* customBox = new QGroupBox(QStringLiteral("扩展属性（自由键值，供后续扩展）"), content);
    auto* customLayout = new QVBoxLayout(customBox);
    m_customTable = new QTableWidget(0, 2, customBox);
    m_customTable->setHorizontalHeaderLabels({ QStringLiteral("属性名"), QStringLiteral("属性值") });
    m_customTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_customTable->verticalHeader()->setVisible(false);
    customLayout->addWidget(m_customTable);

    auto* customButtons = new QHBoxLayout;
    auto* addCustomButton = new QPushButton(QStringLiteral("添加属性…"), customBox);
    auto* removeCustomButton = new QPushButton(QStringLiteral("删除选中行"), customBox);
    customButtons->addWidget(addCustomButton);
    customButtons->addWidget(removeCustomButton);
    customButtons->addStretch();
    customLayout->addLayout(customButtons);
    outer->addWidget(customBox);

    connect(addCustomButton, &QPushButton::clicked, this, &SpeciesForm::addCustomProperty);
    connect(removeCustomButton, &QPushButton::clicked, this, &SpeciesForm::removeCustomProperty);

    // 下拉菜单和数字输入框悬停时不再响应鼠标滚轮，避免误改选项。
    auto* wheelGuard = new WheelIgnoreFilter(content);
    for (QComboBox* combo : content->findChildren<QComboBox*>()) {
        combo->installEventFilter(wheelGuard);
        if (QAbstractItemView* view = combo->view()) {
            view->installEventFilter(wheelGuard);
            if (view->viewport())
                view->viewport()->installEventFilter(wheelGuard);
        }
    }
    for (QAbstractSpinBox* spin : content->findChildren<QAbstractSpinBox*>()) {
        spin->installEventFilter(wheelGuard);
        for (QObject* child : spin->children()) {
            if (auto* childWidget = qobject_cast<QWidget*>(child))
                childWidget->installEventFilter(wheelGuard);
        }
    }

    scroll->setWidget(content);
    return scroll;
}

void SpeciesForm::populateFromInfo(const SpeciesInfo& info)
{
    m_latinEdit->setText(info.scientificName);
    m_aliasList->clear();
    for (const QString& alias : info.aliases) {
        if (!alias.trimmed().isEmpty())
            m_aliasList->addItem(alias.trimmed());
    }
    m_descriptionEdit->setPlainText(info.description);

    m_lightCombo->setCurrentIndex(comboIndexByKey(m_lightCombo, lightToKey(info.light)));
    m_waterCombo->setCurrentIndex(comboIndexByKey(m_waterCombo, waterToKey(info.water)));
    m_tempMin->setValue(info.temperatureMinC);
    m_tempMax->setValue(info.temperatureMaxC);
    m_humidityMin->setValue(info.humidityMinPct);
    m_humidityMax->setValue(info.humidityMaxPct);
    m_phMin->setValue(info.phMin);
    m_phMax->setValue(info.phMax);
    m_zoneLow->setValue(info.hardinessZoneLow);
    m_zoneHigh->setValue(info.hardinessZoneHigh);
    setCheckSet(info.soilTypes, m_soilChecks, predefinedSoilTypes());

    m_habitCombo->setCurrentIndex(comboIndexByKey(m_habitCombo, habitToKey(info.habit)));
    m_lifecycleCombo->setCurrentIndex(
        comboIndexByKey(m_lifecycleCombo, lifecycleToKey(info.lifeCycle)));
    m_foliageCombo->setCurrentIndex(
        comboIndexByKey(m_foliageCombo, foliageToKey(info.foliage)));
    m_growthRateCombo->setCurrentIndex(
        comboIndexByKey(m_growthRateCombo, growthRateToKey(info.growthRate)));
    m_heightMin->setValue(info.heightMinCm);
    m_heightMax->setValue(info.heightMaxCm);
    m_spreadMin->setValue(info.spreadMinCm);
    m_spreadMax->setValue(info.spreadMaxCm);
    setCheckSet(info.propagationMethods, m_propagationChecks, predefinedPropagationMethods());
    setCheckSet(info.usageTags, m_usageChecks, predefinedUsageTags());

    setMonths(info.bloomMonths, m_bloomChecks);
    setMonths(info.fruitMonths, m_fruitChecks);

    m_customTable->setRowCount(0);
    QStringList keys = info.custom.keys();
    keys.sort();
    for (const QString& key : keys) {
        const int row = m_customTable->rowCount();
        m_customTable->insertRow(row);
        m_customTable->setItem(row, 0, new QTableWidgetItem(key));
        m_customTable->setItem(row, 1, new QTableWidgetItem(info.custom.value(key)));
    }

    refreshPhotoList(info);
}

void SpeciesForm::setCheckSet(const QSet<QString>& keys,
                              const QVector<QCheckBox*>& checks,
                              const QStringList& keyOrder)
{
    for (QCheckBox* box : checks) {
        box->setChecked(false);
        const QString key = box->property("key").toString();
        if (keys.contains(key))
            box->setChecked(true);
    }
    Q_UNUSED(keyOrder);
}

void SpeciesForm::setMonths(const QVector<int>& months,
                            const QVector<QCheckBox*>& checks)
{
    for (QCheckBox* box : checks)
        box->setChecked(months.contains(box->property("month").toInt()));
}

bool SpeciesForm::collectFromForm(SpeciesInfo& out, QString& error) const
{
    out.scientificName = m_latinEdit->text().trimmed();

    QStringList aliases;
    for (int i = 0; i < m_aliasList->count(); ++i) {
        const QString alias =
            m_aliasList->item(i)->text().trimmed();
        if (!alias.isEmpty() && !aliases.contains(alias))
            aliases.append(alias);
    }
    out.aliases = aliases;
    out.description = m_descriptionEdit->toPlainText();

    out.light = lightFromKey(m_lightCombo->currentData().toString());
    out.water = waterFromKey(m_waterCombo->currentData().toString());
    out.temperatureMinC = m_tempMin->value();
    out.temperatureMaxC = m_tempMax->value();
    out.humidityMinPct = m_humidityMin->value();
    out.humidityMaxPct = m_humidityMax->value();
    out.phMin = m_phMin->value();
    out.phMax = m_phMax->value();
    out.hardinessZoneLow = m_zoneLow->value();
    out.hardinessZoneHigh = m_zoneHigh->value();

    for (QCheckBox* box : m_soilChecks) {
        if (box->isChecked())
            out.soilTypes.insert(box->property("key").toString());
    }

    out.habit = habitFromKey(m_habitCombo->currentData().toString());
    out.lifeCycle = lifecycleFromKey(m_lifecycleCombo->currentData().toString());
    out.foliage = foliageFromKey(m_foliageCombo->currentData().toString());
    out.growthRate = growthRateFromKey(m_growthRateCombo->currentData().toString());
    out.heightMinCm = m_heightMin->value();
    out.heightMaxCm = m_heightMax->value();
    out.spreadMinCm = m_spreadMin->value();
    out.spreadMaxCm = m_spreadMax->value();

    for (QCheckBox* box : m_propagationChecks) {
        if (box->isChecked())
            out.propagationMethods.insert(box->property("key").toString());
    }
    for (QCheckBox* box : m_usageChecks) {
        if (box->isChecked())
            out.usageTags.insert(box->property("key").toString());
    }

    for (QCheckBox* box : m_bloomChecks) {
        if (box->isChecked())
            out.bloomMonths.append(box->property("month").toInt());
    }
    for (QCheckBox* box : m_fruitChecks) {
        if (box->isChecked())
            out.fruitMonths.append(box->property("month").toInt());
    }
    std::sort(out.bloomMonths.begin(), out.bloomMonths.end());
    std::sort(out.fruitMonths.begin(), out.fruitMonths.end());

    for (int row = 0; row < m_customTable->rowCount(); ++row) {
        const QString key = m_customTable->item(row, 0)
            ? m_customTable->item(row, 0)->text().trimmed()
            : QString();
        if (key.isEmpty())
            continue;
        const QString value = m_customTable->item(row, 1)
            ? m_customTable->item(row, 1)->text()
            : QString();
        out.custom.insert(key, value);
    }

    for (int i = 0; i < m_photoList->count(); ++i) {
        QListWidgetItem* item = m_photoList->item(i);
        const QString fileName = item->data(Qt::UserRole).toString();
        if (!fileName.isEmpty() && !out.photos.contains(fileName))
            out.photos.append(fileName);
    }

    if (out.temperatureMinC > out.temperatureMaxC)
        error = QStringLiteral("温度范围填反了（最低值大于最高值）。");
    else if (out.humidityMinPct > out.humidityMaxPct)
        error = QStringLiteral("空气湿度范围填反了。");
    else if (out.phMin > out.phMax)
        error = QStringLiteral("pH 范围填反了。");
    else if (out.hardinessZoneLow > 0 && out.hardinessZoneHigh > 0
             && out.hardinessZoneLow > out.hardinessZoneHigh)
        error = QStringLiteral("耐寒区范围填反了。");
    else if ((out.heightMinCm > 0 && out.heightMaxCm > 0 && out.heightMinCm > out.heightMaxCm)
             || (out.spreadMinCm > 0 && out.spreadMaxCm > 0
                 && out.spreadMinCm > out.spreadMaxCm))
        error = QStringLiteral("尺寸范围填反了。");
    else
        error.clear();

    return error.isEmpty();
}

bool SpeciesForm::requestSave()
{
    if (!m_document || m_nodeId <= 0) {
        emit infoSaveError(QStringLiteral("尚未选择可编辑的物种节点。"));
        return false;
    }

    SpeciesInfo collected;
    QString error;
    if (!collectFromForm(collected, error)) {
        QMessageBox::warning(this, QStringLiteral("无法保存"), error);
        emit infoSaveError(error);
        return false;
    }

    if (!m_document->setInfo(m_nodeId, collected, &error)) {
        QMessageBox::warning(this, QStringLiteral("无法保存"), error);
        emit infoSaveError(error);
        return false;
    }

    // 保存成功后再清理“本次会话添加但最终没有被引用”的照片文件，
    // 例如用户添加照片后又从资料里移除。
    QStringList stillReferenced;
    const QDir photosDir(QDir(m_dataDir).filePath(QStringLiteral("photos")));
    for (const QString& fileName : m_copiedPhotosThisSession) {
        if (collected.photos.contains(fileName)) {
            stillReferenced.append(fileName);
            continue;
        }
        const QString copied = photosDir.filePath(fileName);
        if (QFile::exists(copied) && !QFile::remove(copied))
            qWarning() << "移除未引用的会话照片失败:" << copied;
    }
    m_copiedPhotosThisSession = stillReferenced;

    const SpeciesInfo* saved = m_document->infoOf(m_nodeId);
    if (saved)
        populateFromInfo(*saved);
    emit infoSaved();
    return true;
}

void SpeciesForm::addAlias()
{
    bool ok = false;
    const QString alias = QInputDialog::getText(
        this, QStringLiteral("添加别名"),
        QStringLiteral("请输入一个别名 / 俗名（每次只添加一条，例如：玫瑰花）："),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || alias.isEmpty())
        return;

    if (alias.contains(QRegularExpression(QStringLiteral("[,，;；/、]")))) {
        QMessageBox::warning(
            this, QStringLiteral("请逐条添加"),
            QStringLiteral("一个别名只填一条，请不要在里面使用逗号、分号或斜杠分隔。"));
        return;
    }

    for (int i = 0; i < m_aliasList->count(); ++i) {
        if (m_aliasList->item(i)->text().trimmed() == alias) {
            m_aliasList->setCurrentRow(i);
            return;
        }
    }
    m_aliasList->addItem(alias);
    m_aliasList->setCurrentRow(m_aliasList->count() - 1);
}

void SpeciesForm::removeSelectedAlias()
{
    const int row = m_aliasList->currentRow();
    if (row >= 0)
        delete m_aliasList->takeItem(row);
}

bool SpeciesForm::hasUnsavedChanges() const
{
    if (!m_document || m_nodeId <= 0)
        return false;

    const TaxonNode* n = m_document->node(m_nodeId);
    const SpeciesInfo baseline =
        (n && n->hasInfo) ? n->info : SpeciesInfo();
    SpeciesInfo current;
    QString error;
    if (!collectFromForm(current, error))
        return true;
    return !(current == baseline);
}

QString SpeciesForm::resolvePhotoPath(const QString& relativeName) const
{
    if (!isSafePhotoFileName(relativeName))
        return QString();
    return QDir(m_dataDir).filePath(QStringLiteral("photos") + QLatin1Char('/') + relativeName);
}

void SpeciesForm::refreshPhotoList(const SpeciesInfo& info)
{
    m_photoList->clear();
    QString firstPhoto;
    for (const QString& relativeName : info.photos) {
        const QString fullPath = resolvePhotoPath(relativeName);
        auto* item = new QListWidgetItem(QFileInfo(relativeName).fileName());
        item->setData(Qt::UserRole, relativeName);
        item->setToolTip(fullPath);
        QPixmap pixmap(fullPath);
        if (!pixmap.isNull()) {
            item->setIcon(QIcon(pixmap.scaled(
                QSize(88, 88), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        } else {
            item->setText(relativeName + QStringLiteral("（缺失）"));
        }
        if (firstPhoto.isEmpty())
            firstPhoto = relativeName;
        m_photoList->addItem(item);
    }

    // 有照片时默认高亮第一张并展示；没有则显示占位提示。
    if (m_photoList->count() > 0) {
        const QSignalBlocker blocker(m_photoList);
        m_photoList->setCurrentRow(0);
    }
    setPhotoPreview(firstPhoto);
}

void SpeciesForm::onPhotoSelectionChanged(QListWidgetItem* current,
                                         QListWidgetItem* previous)
{
    Q_UNUSED(previous);
    if (m_loading)
        return;
    setPhotoPreview(current ? current->data(Qt::UserRole).toString() : QString());
}

void SpeciesForm::setPhotoPreview(const QString& relativeName)
{
    if (relativeName.isEmpty()) {
        m_photoPreviewOriginal = QPixmap();
    } else {
        const QPixmap loaded(resolvePhotoPath(relativeName));
        m_photoPreviewOriginal = loaded.isNull() ? QPixmap() : loaded;
    }
    updatePhotoPreview();
}

void SpeciesForm::updatePhotoPreview()
{
    if (!m_photoPreview)
        return;

    if (m_photoPreviewOriginal.isNull()) {
        m_photoPreview->setText(QStringLiteral("暂无照片\n\n"
                                               "可点击下方“添加照片…”为这个植物配图。"));
        m_photoPreview->setPixmap(QPixmap());
        return;
    }

    QSize area = m_photoPreview->contentsRect().size();
    if (area.width() <= 0 || area.height() <= 0)
        area = m_photoPreview->size();
    m_photoPreview->setText(QString());
    m_photoPreview->setPixmap(m_photoPreviewOriginal.scaled(
        area, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void SpeciesForm::resizeEvent(QResizeEvent* event)
{
    updatePhotoPreview();
    QWidget::resizeEvent(event);
}

void SpeciesForm::addPhotos()
{
    if (m_nodeId <= 0)
        return;
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择照片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)"));
    if (files.isEmpty())
        return;

    QDir photosDir(QDir(m_dataDir).filePath(QStringLiteral("photos")));
    if (!photosDir.exists() && !QDir().mkpath(photosDir.absolutePath())) {
        QMessageBox::warning(this, QStringLiteral("无法创建照片目录"),
                             QStringLiteral("无法创建：%1").arg(photosDir.absolutePath()));
        return;
    }

    QStringList added;
    for (const QString& source : files) {
        const QFileInfo sourceInfo(source);
        const QString base = sourceInfo.completeBaseName();
        const QString suffix = sourceInfo.suffix();
        const qint64 stamp = QDateTime::currentMSecsSinceEpoch();

        QString candidate = QStringLiteral("node%1_%2_%3")
                                .arg(m_nodeId).arg(stamp).arg(base);
        if (!suffix.isEmpty())
            candidate += QLatin1Char('.') + suffix;

        int counter = 0;
        while (QFileInfo(photosDir.filePath(candidate)).exists()) {
            candidate = QStringLiteral("node%1_%2_%3_%4")
                            .arg(m_nodeId).arg(stamp).arg(base).arg(++counter);
            if (!suffix.isEmpty())
                candidate += QLatin1Char('.') + suffix;
        }

        if (QFile::copy(source, photosDir.filePath(candidate)))
            added.append(candidate);
        else
            qWarning() << "复制照片失败:" << source;
    }

    if (added.isEmpty())
        return;

    const TaxonNode* n = m_document ? m_document->node(m_nodeId) : nullptr;
    SpeciesInfo info;
    if (n && n->hasInfo)
        info = n->info;
    info.photos.append(added);
    QString error;
    if (!m_document->setInfo(m_nodeId, info, &error)) {
        // 写入数据库失败时，清理本次已经复制到照片目录的文件，避免残留。
        for (const QString& fileName : added) {
            const QString copied = photosDir.filePath(fileName);
            if (QFile::exists(copied))
                QFile::remove(copied);
        }
        QMessageBox::warning(this, QStringLiteral("无法保存照片信息"), error);
        return;
    }
    for (const QString& fileName : added) {
        if (!m_copiedPhotosThisSession.contains(fileName))
            m_copiedPhotosThisSession.append(fileName);
    }
    refreshPhotoList(info);
    emit infoSaved();
}

void SpeciesForm::removeSelectedPhotos()
{
    const QList<QListWidgetItem*> selected = m_photoList->selectedItems();
    if (selected.isEmpty())
        return;

    const TaxonNode* n = m_document ? m_document->node(m_nodeId) : nullptr;
    if (!n || !n->hasInfo)
        return;

    SpeciesInfo info = n->info;
    for (const QListWidgetItem* item : selected) {
        const QString relativeName = item->data(Qt::UserRole).toString();
        info.photos.removeAll(relativeName);
    }

    QString error;
    if (!m_document->setInfo(m_nodeId, info, &error)) {
        QMessageBox::warning(this, QStringLiteral("无法保存照片信息"), error);
        return;
    }
    refreshPhotoList(info);
    emit infoSaved();
}

void SpeciesForm::openSelectedPhoto()
{
    QListWidgetItem* item = m_photoList->currentItem();
    if (!item)
        return;
    const QString path = resolvePhotoPath(item->data(Qt::UserRole).toString());
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("文件不存在"),
                             QStringLiteral("找不到图片文件：\n%1").arg(path));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void SpeciesForm::addCustomProperty()
{
    bool ok = false;
    const QString key = QInputDialog::getText(
        this, QStringLiteral("添加扩展属性"),
        QStringLiteral("属性名（例如：耐盐碱性、入侵等级）："), QLineEdit::Normal,
        QString(), &ok);
    if (!ok || key.trimmed().isEmpty())
        return;

    const QString cleanKey = key.trimmed();
    const QString value = QInputDialog::getText(
        this, QStringLiteral("添加扩展属性"),
        QStringLiteral("“%1”的值：").arg(cleanKey), QLineEdit::Normal,
        QString(), &ok);
    if (!ok)
        return;

    for (int row = 0; row < m_customTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_customTable->item(row, 0);
        if (item && item->text() == cleanKey) {
            m_customTable->setItem(row, 1, new QTableWidgetItem(value));
            return;
        }
    }

    const int row = m_customTable->rowCount();
    m_customTable->insertRow(row);
    m_customTable->setItem(row, 0, new QTableWidgetItem(cleanKey));
    m_customTable->setItem(row, 1, new QTableWidgetItem(value));
}

void SpeciesForm::removeCustomProperty()
{
    const int row = m_customTable->currentRow();
    if (row >= 0)
        m_customTable->removeRow(row);
}

void SpeciesForm::markEdited()
{
    // 将来若改为“自动保存”，可在这里做防抖并触发 infoSaved。
}
