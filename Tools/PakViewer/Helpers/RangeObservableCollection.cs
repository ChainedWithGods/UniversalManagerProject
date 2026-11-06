using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.ComponentModel;

namespace PakViewer.Helpers;

public class RangeObservableCollection<T> : ObservableCollection<T>
{
    private bool _suppressNotification = false;

    public RangeObservableCollection() : base() { }
    public RangeObservableCollection(IEnumerable<T> collection) : base(collection) { }
    public RangeObservableCollection(List<T> list) : base(list) { }

    protected override void OnCollectionChanged(NotifyCollectionChangedEventArgs e)
    {
        if (!_suppressNotification)
            base.OnCollectionChanged(e);
    }

    public void AddRange(IEnumerable<T> list)
    {
        if (list == null) throw new ArgumentNullException(nameof(list));

        _suppressNotification = true;

        foreach (T item in list)
        {
            Add(item);
        }

        _suppressNotification = false;
        OnCollectionChanged(new NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction.Reset));
    }

    public void ReplaceRange(IEnumerable<T> list)
    {
        if (list == null) throw new ArgumentNullException(nameof(list));

        _suppressNotification = true;

        Clear();
        foreach (T item in list)
        {
            Add(item);
        }

        _suppressNotification = false;
        OnCollectionChanged(new NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction.Reset));
    }
}
