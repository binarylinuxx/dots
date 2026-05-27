#include "plugin_registry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

PluginRegistry::PluginRegistry(QObject* parent) : QAbstractListModel(parent) {
	const QByteArray envDirs = qgetenv("BLXSHELL_PLUGIN_DIRS");
	if (!envDirs.isEmpty()) {
		const QStringList rawDirs = QString::fromLocal8Bit(envDirs).split(QLatin1Char(':'), Qt::SkipEmptyParts);
		for (const QString& dir : rawDirs) {
			m_pluginDirs.append(QDir::cleanPath(dir));
		}
	}

	if (m_pluginDirs.isEmpty()) {
		const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
		if (!dataHome.isEmpty()) {
			m_pluginDirs.append(QDir(dataHome).filePath(QStringLiteral("blxshell/plugins")));
		}

		const QString blxshellPath = QString::fromLocal8Bit(qgetenv("BLXSHELL_PATH"));
		if (!blxshellPath.isEmpty()) {
			m_pluginDirs.append(QDir(blxshellPath).filePath(QStringLiteral("plugins")));
		}
	}

	m_pluginDirs.removeDuplicates();
	m_pluginsDir = m_pluginDirs.join(QLatin1Char(':'));
	rescan();
}

void PluginRegistry::setPluginsDir(const QString& dir) {
	setPluginDirs(dir.split(QLatin1Char(':'), Qt::SkipEmptyParts));
}

void PluginRegistry::setPluginDirs(const QStringList& dirs) {
	QStringList cleaned;
	for (const QString& dir : dirs) {
		if (!dir.isEmpty()) cleaned.append(QDir::cleanPath(dir));
	}
	cleaned.removeDuplicates();

	if (m_pluginDirs == cleaned) return;
	m_pluginDirs = cleaned;
	m_pluginsDir = m_pluginDirs.join(QLatin1Char(':'));
	emit pluginsDirChanged();
	emit pluginDirsChanged();
	rescan();
}

int PluginRegistry::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) return 0;
	return m_plugins.size();
}

QHash<int, QByteArray> PluginRegistry::roleNames() const {
	return {
		{ IdRole,             "id" },
		{ NameRole,           "name" },
		{ VersionRole,        "version" },
		{ AuthorRole,         "author" },
		{ DescriptionRole,    "description" },
		{ IconRole,           "icon" },
		{ PathRole,           "path" },
		{ ProvidesRole,       "provides" },
		{ ProvidesKindsRole,  "providesKinds" },
		{ ConfigSchemaRole,   "configSchema" },
		{ ValidRole,          "valid" },
		{ ErrorRole,          "error" },
	};
}

QVariant PluginRegistry::data(const QModelIndex& index, int role) const {
	if (!index.isValid() || index.row() < 0 || index.row() >= m_plugins.size())
		return {};

	const PluginEntry& p = m_plugins.at(index.row());
	switch (role) {
		case IdRole:            return p.id;
		case NameRole:          return p.name;
		case VersionRole:       return p.version;
		case AuthorRole:        return p.author;
		case DescriptionRole:   return p.description;
		case IconRole:          return p.icon;
		case PathRole:          return p.path;
		case ProvidesRole:      return p.provides;
		case ProvidesKindsRole: return p.providesKinds;
		case ConfigSchemaRole:  return p.configSchema;
		case ValidRole:         return p.valid;
		case ErrorRole:         return p.error;
		default:                return {};
	}
}

PluginEntry PluginRegistry::parseManifest(const QString& manifestPath) const {
	PluginEntry e;
	e.path = QFileInfo(manifestPath).absolutePath();

	QFile f(manifestPath);
	if (!f.open(QIODevice::ReadOnly)) {
		e.error = QStringLiteral("cannot open manifest: %1").arg(f.errorString());
		return e;
	}

	QJsonParseError pe;
	const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
	if (pe.error != QJsonParseError::NoError) {
		e.error = QStringLiteral("JSON parse error: %1 at offset %2")
			.arg(pe.errorString()).arg(pe.offset);
		return e;
	}
	if (!doc.isObject()) {
		e.error = QStringLiteral("manifest root must be an object");
		return e;
	}

	const QJsonObject o = doc.object();

	// required fields
	e.id      = o.value(QStringLiteral("id")).toString();
	e.name    = o.value(QStringLiteral("name")).toString();

	if (e.id.isEmpty()) {
		e.error = QStringLiteral("missing 'id'");
		return e;
	}
	if (e.name.isEmpty()) {
		e.error = QStringLiteral("missing 'name'");
		return e;
	}

	// optional fields
	e.version     = o.value(QStringLiteral("version")).toString();
	e.author      = o.value(QStringLiteral("author")).toString();
	e.description = o.value(QStringLiteral("description")).toString();
	e.icon        = o.value(QStringLiteral("icon")).toString();

	// provides: resolve relative component paths to absolute
	const QJsonValue provVal = o.value(QStringLiteral("provides"));
	if (provVal.isObject()) {
		const QJsonObject provObj = provVal.toObject();
		QVariantMap resolved;
		for (auto it = provObj.begin(); it != provObj.end(); ++it) {
			e.providesKinds.append(it.key());

			QVariant v = it.value().toVariant();
			// shorthand: "bar": "BarItem.qml" -> {"component": "BarItem.qml"}
			if (v.typeId() == QMetaType::QString) {
				QVariantMap m;
				m.insert(QStringLiteral("component"), v.toString());
				v = m;
			}
			if (v.typeId() == QMetaType::QVariantMap) {
				QVariantMap m = v.toMap();
				const QString comp = m.value(QStringLiteral("component")).toString();
				if (!comp.isEmpty()) {
					const QString abs = QDir(e.path).absoluteFilePath(comp);
					m.insert(QStringLiteral("componentUrl"),
						QUrl::fromLocalFile(abs).toString());
				}
				resolved.insert(it.key(), m);
			} else {
				resolved.insert(it.key(), v);
			}
		}
		e.provides = resolved;
	}

	// config schema: kept as-is for QML to render
	const QJsonValue cfgVal = o.value(QStringLiteral("config"));
	if (cfgVal.isObject()) {
		e.configSchema = cfgVal.toObject().toVariantMap();
	}

	e.valid = true;
	return e;
}

void PluginRegistry::rescan() {
	beginResetModel();
	m_plugins.clear();

	for (const QString& dir : m_pluginDirs) {
		QDir root(dir);
		if (!root.exists()) continue;

		const QStringList dirs = root.entryList(
			QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		for (const QString& sub : dirs) {
			const QString manifest = root.filePath(sub + QStringLiteral("/plugin.json"));
			if (!QFile::exists(manifest)) continue;
			m_plugins.append(parseManifest(manifest));
		}
	}
	endResetModel();
	emit countChanged();
	emit rescanned();
}

QVariantMap PluginRegistry::get(const QString& id) const {
	for (const auto& p : m_plugins) {
		if (p.id == id) {
			QVariantMap m;
			m.insert(QStringLiteral("id"),            p.id);
			m.insert(QStringLiteral("name"),          p.name);
			m.insert(QStringLiteral("version"),       p.version);
			m.insert(QStringLiteral("author"),        p.author);
			m.insert(QStringLiteral("description"),   p.description);
			m.insert(QStringLiteral("icon"),          p.icon);
			m.insert(QStringLiteral("path"),          p.path);
			m.insert(QStringLiteral("provides"),      p.provides);
			m.insert(QStringLiteral("providesKinds"), p.providesKinds);
			m.insert(QStringLiteral("configSchema"),  p.configSchema);
			m.insert(QStringLiteral("valid"),         p.valid);
			m.insert(QStringLiteral("error"),         p.error);
			return m;
		}
	}
	return {};
}

QVariantList PluginRegistry::byKind(const QString& kind) const {
	QVariantList out;
	for (const auto& p : m_plugins) {
		if (!p.valid) continue;
		if (p.providesKinds.contains(kind)) {
			QVariantMap m = get(p.id);
			m.insert(QStringLiteral("kindData"), p.provides.value(kind));
			out.append(m);
		}
	}
	return out;
}
