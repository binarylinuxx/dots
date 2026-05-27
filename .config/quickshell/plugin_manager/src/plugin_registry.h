#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

struct PluginEntry {
	QString id;
	QString name;
	QString version;
	QString author;
	QString description;
	QString icon;
	QString path;                 // absolute plugin dir
	QVariantMap provides;         // { bar: {...}, background: {...}, ... }
	QStringList providesKinds;    // ["bar","background","sidebarTile", ...]
	QVariantMap configSchema;     // raw "config" object from manifest
	bool valid = false;
	QString error;                // parse/validation error (empty if ok)
};

class PluginRegistry : public QAbstractListModel {
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(QString pluginsDir READ pluginsDir WRITE setPluginsDir NOTIFY pluginsDirChanged)
	Q_PROPERTY(QStringList pluginDirs READ pluginDirs WRITE setPluginDirs NOTIFY pluginDirsChanged)
	Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
	enum Roles {
		IdRole = Qt::UserRole + 1,
		NameRole,
		VersionRole,
		AuthorRole,
		DescriptionRole,
		IconRole,
		PathRole,
		ProvidesRole,
		ProvidesKindsRole,
		ConfigSchemaRole,
		ValidRole,
		ErrorRole,
	};

	explicit PluginRegistry(QObject* parent = nullptr);

	QString pluginsDir() const { return m_pluginsDir; }
	void setPluginsDir(const QString& dir);
	QStringList pluginDirs() const { return m_pluginDirs; }
	void setPluginDirs(const QStringList& dirs);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	// QML API
	Q_INVOKABLE void rescan();
	Q_INVOKABLE QVariantMap get(const QString& id) const;
	Q_INVOKABLE QVariantList byKind(const QString& kind) const;

signals:
	void pluginsDirChanged();
	void pluginDirsChanged();
	void countChanged();
	void rescanned();

private:
	PluginEntry parseManifest(const QString& manifestPath) const;

	QString m_pluginsDir;
	QStringList m_pluginDirs;
	QVector<PluginEntry> m_plugins;
};
